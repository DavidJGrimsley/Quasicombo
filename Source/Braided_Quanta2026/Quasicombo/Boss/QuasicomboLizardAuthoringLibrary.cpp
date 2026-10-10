#include "QuasicomboLizardAuthoringLibrary.h"
#if WITH_EDITOR
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Animation/MorphTarget.h"
#include "Animation/AnimSequence.h"
#include "AssetCompilingManager.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#endif

void UQuasicomboLizardAuthoringLibrary::FinishAnimationCompression(UAnimSequence* Animation)
{
#if WITH_EDITOR
	FAssetCompilingManager::Get().FinishAllCompilation();
#endif
}

bool UQuasicomboLizardAuthoringLibrary::ExportReferencePose(USkeletalMesh* Mesh, const FString& Filename)
{
#if WITH_EDITOR
	if (!Mesh) return false;
	const auto& Ref = Mesh->GetRefSkeleton();
	TArray<TSharedPtr<FJsonValue>> Bones;
	TArray<FTransform> World;
	for (int32 I=0; I<Ref.GetNum(); ++I)
	{
		const int32 Parent = Ref.GetParentIndex(I);
		World.Add(Parent == INDEX_NONE ? Ref.GetRefBonePose()[I] : Ref.GetRefBonePose()[I] * World[Parent]);
		auto Bone = MakeShared<FJsonObject>();
		Bone->SetStringField(TEXT("name"),Ref.GetBoneName(I).ToString());
		Bone->SetStringField(TEXT("parent"),Parent == INDEX_NONE ? TEXT("") : Ref.GetBoneName(Parent).ToString());
		TArray<TSharedPtr<FJsonValue>> T,Q;
		const FVector P = World[I].GetLocation(); const FQuat R = World[I].GetRotation();
		for (double V : {P.X,P.Y,P.Z}) T.Add(MakeShared<FJsonValueNumber>(V));
		for (double V : {R.X,R.Y,R.Z,R.W}) Q.Add(MakeShared<FJsonValueNumber>(V));
		Bone->SetArrayField(TEXT("t"),T);Bone->SetArrayField(TEXT("q"),Q);
		Bones.Add(MakeShared<FJsonValueObject>(Bone));
	}
	auto Root=MakeShared<FJsonObject>();Root->SetArrayField(TEXT("bones"),Bones);
	FString Json;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));
	return FFileHelper::SaveStringToFile(Json,*Filename);
#else
	return false;
#endif
}
bool UQuasicomboLizardAuthoringLibrary::AddFaceShapes(USkeletalMesh* Original, USkeletalMesh* Duplicate, const FString& Filename)
{
#if WITH_EDITOR
	if (!Original || !Duplicate || Original == Duplicate || !Duplicate->GetImportedModel()) return false;
	TArray<FSoftSkinVertex> SourceVertices,Vertices;
	Original->GetImportedModel()->LODModels[0].GetVertices(SourceVertices);
	auto& LOD=Duplicate->GetImportedModel()->LODModels[0];LOD.GetVertices(Vertices);
	if (Vertices.Num()!=SourceVertices.Num() || Duplicate->GetSkeleton()!=Original->GetSkeleton()) return false;
	for (int32 I=0; I<Vertices.Num(); ++I)
	{
		const auto& A=Vertices[I]; const auto& B=SourceVertices[I];
		if (A.Position!=B.Position || FMemory::Memcmp(A.InfluenceWeights,B.InfluenceWeights,sizeof(A.InfluenceWeights)) ||
			FMemory::Memcmp(A.InfluenceBones,B.InfluenceBones,sizeof(A.InfluenceBones))) return false;
	}
	FString Text;TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text,*Filename) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root)) return false;
	struct FSample { FVector3f P,Smirk,Narrow; };
	TMap<FIntVector,TArray<FSample>> Samples;
	auto Grid=[](FVector3f P) {return FIntVector(FMath::RoundToInt(P.X*100),FMath::RoundToInt(P.Y*100),FMath::RoundToInt(P.Z*100));};
	auto Read=[](const TSharedPtr<FJsonObject>& V,const TCHAR* Name) {const auto& A=V->GetArrayField(Name);return FVector3f(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber());};
	for (const auto& V:Root->GetArrayField(TEXT("vertices")))
	{
		FSample S;S.P=Read(V->AsObject(),TEXT("p"));S.Smirk=Read(V->AsObject(),TEXT("smirk"));S.Narrow=Read(V->AsObject(),TEXT("narrow"));
		Samples.FindOrAdd(Grid(S.P)).Add(S);
	}
	TArray<FMorphTargetDelta> Smirk,Narrow;
	int32 Matched=0;
	for (int32 I=0;I<Vertices.Num();++I)
	{
		const auto P=Vertices[I].Position; const auto Cell=Grid(P);
		const FSample* Best=nullptr;float BestSquared=0.0001f;
		for (int32 X=-1;X<=1;++X) for (int32 Y=-1;Y<=1;++Y) for(int32 Z=-1;Z<=1;++Z)
			if (const auto* Bucket=Samples.Find(Cell+FIntVector(X,Y,Z)))
				for (const auto& Sample:*Bucket)
					if ((Sample.P-P).SizeSquared()<BestSquared) {Best=&Sample;BestSquared=(Sample.P-P).SizeSquared();}
		if (!Best) continue;
		++Matched;
		for (int32 J=0;J<2;++J)
		{
			const auto D=J==0?Best->Smirk:Best->Narrow;
			if (D.SizeSquared()<1.e-10f) continue;
			FMorphTargetDelta Delta;Delta.SourceIdx=I;Delta.PositionDelta=D;Delta.TangentZDelta=FVector3f::ZeroVector;
			(J==0?Smirk:Narrow).Add(Delta);
		}
	}
	if (Matched!=Vertices.Num() || Smirk.IsEmpty() || Narrow.IsEmpty())
	{
		UE_LOG(LogTemp,Error,TEXT("Lizard facial mapping: %d/%d vertices, %d smirk, %d eyelid"),Matched,Vertices.Num(),Smirk.Num(),Narrow.Num());
		return false;
	}
	Duplicate->Modify();
	for (int32 J=0;J<2;++J)
	{
		const FName MorphName=J==0?TEXT("EvolutionSmirk"):TEXT("EvolutionEyeNarrow");
		if (auto* Existing=Duplicate->FindMorphTarget(MorphName)) Duplicate->UnregisterMorphTarget(Existing,false);
		auto* Morph=NewObject<UMorphTarget>(Duplicate,J==0?TEXT("EvolutionSmirk"):TEXT("EvolutionEyeNarrow"));
		Morph->PopulateDeltas(J==0?Smirk:Narrow,0,LOD.Sections,false,true,0.0001f);
		Duplicate->RegisterMorphTarget(Morph,false);
	}
	// Anchors are computed in the actual mesh's head space, not the shared human reference pose.
	const auto& Ref=Duplicate->GetRefSkeleton();
	FTransform Head=FTransform::Identity;
	TArray<int32> Chain;for(int32 I=Ref.FindBoneIndex(TEXT("head"));I!=INDEX_NONE;I=Ref.GetParentIndex(I))Chain.Add(I);
	for(int32 I=Chain.Num()-1;I>=0;--I)Head=Ref.GetRefBonePose()[Chain[I]]*Head;
	for (int32 J=0;J<2;++J)
	{
		const FName SocketName=J==0?TEXT("EvolutionEyes"):TEXT("EvolutionFace");
		Duplicate->GetMeshOnlySocketList().RemoveAll([SocketName](const auto& Socket){ return Socket && Socket->SocketName==SocketName; });
		auto* Socket=NewObject<USkeletalMeshSocket>(Duplicate);
		Socket->SocketName=J==0?TEXT("EvolutionEyes"):TEXT("EvolutionFace");Socket->BoneName=TEXT("head");
		Socket->RelativeLocation=Head.InverseTransformPosition(J==0?FVector(0,15.0,177.0):FVector(0,14.0,170.0));
		Duplicate->GetMeshOnlySocketList().Add(Socket);
	}
	Duplicate->InitMorphTargetsAndRebuildRenderData();Duplicate->MarkPackageDirty();
	UE_LOG(LogTemp,Display,TEXT("Lizard facial copy verified: %d unchanged neutral vertices and weights; added %d/%d facial deltas"),Vertices.Num(),Smirk.Num(),Narrow.Num());
	return true;
#else
	return false;
#endif
}
