"""Blender 5 source authoring. Run after ExportLizardAuthoring.py, then import in UE.

All distances are centimeters in the exported armature. The two-bone solver
constrains limb lengths and puts the elbow on a named outward pole. No generic
prefix rotations are applied to twist bones. Keep the .blend as editable source.
"""
import bpy, math, json
from pathlib import Path
from mathutils import Vector, Matrix, Quaternion

PROJECT = Path(__file__).resolve().parents[1]
OUT = PROJECT/'Saved/LizardRebuild'
SOURCE = PROJECT/'Project/AnimationSource'
OUT.mkdir(parents=True,exist_ok=True);SOURCE.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.preferences.filepaths.save_version=0
bpy.ops.import_scene.fbx(filepath=str(OUT/'LizardBody.fbx'),use_anim=False,automatic_bone_orientation=False)
rig = next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
body = next(o for o in bpy.context.scene.objects if o.type=='MESH')
rig.name='root'
body.name='SK_Lizard_EvolutionBody'
rest = {b.name:b.matrix_local.copy() for b in rig.data.bones}
scene=bpy.context.scene
scene.render.fps=60

def update(): bpy.context.view_layer.update()
def ease(t):
    t=max(0.,min(1.,t));return t*t*(3.-2.*t)
def lerp(a,b,t): return a+(b-a)*t
def reset():
    for p in rig.pose.bones:
        p.matrix_basis=Matrix.Identity(4)
    update()
def turn(name, axis, degrees):
    p=rig.pose.bones[name]
    m=p.matrix.copy()
    q=Quaternion(Vector(axis),math.radians(degrees)) @ m.to_quaternion()
    p.matrix=Matrix.LocRotScale(m.translation,q,m.to_scale())
    update()
def aim(name, child, point):
    p=rig.pose.bones[name]
    origin=p.matrix.translation.copy()
    current=rig.pose.bones[child].matrix.translation-origin
    target=Vector(point)-origin
    if current.length<1e-5 or target.length<1e-5:return
    q=current.rotation_difference(target) @ p.matrix.to_quaternion()
    p.matrix=Matrix.LocRotScale(origin,q,p.matrix.to_scale());update()
def limb(upper,lower,end,target,pole):
    a=rig.pose.bones[upper].matrix.translation.copy()
    l1=(rest[lower].translation-rest[upper].translation).length
    l2=(rest[end].translation-rest[lower].translation).length
    direction=Vector(target)-a
    d=max(abs(l1-l2)+.01,min(direction.length,l1+l2-.02))
    u=direction.normalized();v=Vector(pole)-a;v=(v-u*v.dot(u)).normalized()
    along=(l1*l1-l2*l2+d*d)/(2*d)
    elbow=a+u*along+v*math.sqrt(max(0.,l1*l1-along*along))
    aim(upper,lower,elbow);aim(lower,end,a+u*d)
def fist(side, amount):
    # Hinge axes follow the actual metacarpal fan, then each phalanx's own pose.
    names=['index','middle','ring','pinky']
    for finger in names:
        for joint,angle in [(1,58),(2,82),(3,62)]:
            name=f'{finger}_{joint:02d}_{side}'
            p=rig.pose.bones.get(name)
            if not p:continue
            # Both imported hands use the same phalanx flexion sign despite mirrored arm axes.
            p.rotation_mode='QUATERNION'
            p.rotation_quaternion=Quaternion((0,0,1),math.radians(angle*amount))
            update()
    for joint,angle in [(1,18),(2,38),(3,35)]:
        p=rig.pose.bones.get(f'thumb_{joint:02d}_{side}')
        if p:
            p.rotation_mode='QUATERNION'
            p.rotation_quaternion=Quaternion((0,0,1),math.radians(angle*amount))
            update()

# Shape keys change only skin around the lip corner and upper eyelids; the
# neutral Basis, eyes, teeth, skeleton, weights, and UVs remain untouched.
body.shape_key_add(name='Basis')
smirk=body.shape_key_add(name='EvolutionSmirk')
narrow=body.shape_key_add(name='EvolutionEyeNarrow')
# Teeth and eyeballs are disconnected islands. Shape only the large skin surface.
adj=[set() for _ in body.data.vertices]
for edge in body.data.edges:
    a,b=edge.vertices;adj[a].add(b);adj[b].add(a)
skin=set();visited=set()
for i in range(len(adj)):
    if i in visited:continue
    stack=[i];island=set();visited.add(i)
    while stack:
        v=stack.pop();island.add(v)
        for j in adj[v]:
            if j not in visited:visited.add(j);stack.append(j)
    if len(island)>500:skin.update(island)
eye_vertices={i for p in body.data.polygons if 'Eye' in body.data.materials[p.material_index].name for i in p.vertices}
eye_points=[body.data.vertices[i].co for i in eye_vertices]
eye_center=sum(eye_points,Vector())/max(1,len(eye_points))
print('FACE_EYES',list(eye_center),'range',[(min(v[j] for v in eye_points),max(v[j] for v in eye_points)) for j in range(3)])
for v in body.data.vertices:
    x,y,z=v.co
    if v.index in eye_vertices or v.index not in skin:continue
    head_weight=sum(g.weight for g in v.groups if body.vertex_groups[g.group].name=='head')
    jaw_weight=sum(g.weight for g in v.groups if body.vertex_groups[g.group].name=='u_Jaw')
    # Compact smooth ellipsoidal brush, intentionally asymmetric on character left.
    r=((x-7.0)/6.0)**2+((y+16.0)/9.0)**2+((z-167.0)/6.0)**2
    w=max(0.,1.-r)**2 * min(1.,head_weight+jaw_weight)
    smirk.data[v.index].co += Vector((1.1*w,-.4*w,2.5*w))
    ex=abs(x);r=((ex-abs(eye_center.x or 5.5))/5.5)**2
    # Eyes are symmetric about X; use their lateral half-centers below.
    xc=sum(abs(p.x) for p in eye_points)/max(1,len(eye_points))
    r=((ex-xc)/5.0)**2+((y-eye_center.y)/5.0)**2+((z-(eye_center.z+2.3))/4.0)**2
    w=max(0.,1.-r)**2 * head_weight
    narrow.data[v.index].co += Vector(((-.25 if x>0 else .25)*w,-.25*w,-1.15*w))

def ue_vector(v):return [v.x,-v.y,v.z]
(OUT/'facial_shapes.json').write_text(json.dumps({'vertices':[
    dict(p=ue_vector(v.co),smirk=ue_vector(smirk.data[v.index].co-v.co),narrow=ue_vector(narrow.data[v.index].co-v.co))
    for v in body.data.vertices]}))

def pose(coil=0.,assembly=0.,roar=0.,settle=0.,smile=0.,t=0.,reaction=0.):
    reset()
    # Wider, deliberate stance. In-place elevation is supplied by the UE component.
    pelvis=rig.pose.bones['pelvis'];m=pelvis.matrix.copy()
    m.translation+=Vector((0.,-1.2*coil,-3.0*coil-1.7*settle))
    pelvis.matrix=m;update()
    for name,deg in [('spine_01',3.0),('spine_02',3.0),('spine_03',2.0)]:
        turn(name,(1,0,0),deg*coil-2.0*roar+reaction*1.1)
    turn('neck_01',(1,0,0),3.0*coil-2.0*roar)
    turn('head',(1,0,0),5.0*coil-12.0*roar+4.0*smile)
    # The lizard bind pose has an open mouth. Close it around the actual transverse hinge.
    turn('u_Jaw',(1,0,0),-27.0+29.0*roar+1.0*coil)
    for side,sign in [('l',1.),('r',-1.)]:
        # Fixed foot targets, toes forward; the solver supplies controlled knee flexion.
        foot=rest['foot_'+side].translation.copy()+Vector((sign*3.0,-1.,0.))
        limb('thigh_'+side,'calf_'+side,'foot_'+side,foot,(sign*20.,-60.,45.))
        p=rig.pose.bones['foot_'+side]
        p.matrix=Matrix.LocRotScale(p.matrix.translation,rest[p.name].to_quaternion(),Vector((1,1,1)));update()
        turn('clavicle_'+side,(0,1,0),sign*(2.5*coil-2.*roar))
        target=Vector((sign*38.,-10.,94.))
        target=target.lerp(Vector((sign*29.,-26.,119.)),coil)
        target=target.lerp(Vector((sign*42.,-19.,106.)),assembly)
        target=target.lerp(Vector((sign*45.,-16.,93.)),roar)
        target.z+=.45*math.sin(t*7.0)*coil + .16*math.sin(t*41.0)*coil
        limb('upperarm_'+side,'lowerarm_'+side,'hand_'+side,target,(sign*85.,-6.,109.))
        # Keep wrists straight: orient the hand along the forearm using the actual fan direction.
        hand=rig.pose.bones['hand_'+side]
        fingers=[rig.pose.bones[f+'_01_'+side].matrix.translation for f in ['index','middle','ring','pinky']]
        fan=sum(fingers,Vector())/4.-hand.matrix.translation
        forearm=hand.matrix.translation-rig.pose.bones['lowerarm_'+side].matrix.translation
        q=fan.rotation_difference(forearm) @ hand.matrix.to_quaternion()
        hand.matrix=Matrix.LocRotScale(hand.matrix.translation,q,Vector((1,1,1)));update()
        fist(side,.9)
    for i in range(1,8):
        name='u_Tail_%02d'%i
        turn(name,(0,0,1),1.6*math.sin(t*3.-i*.45)*(0.5+coil))
    update()

clips=[('EvolutionRise',1.25),('EvolutionHover',1.),('ArmorAssembly',2.3),('ArmorAttach',.28),('HeroicLand',.85),('EvolutionRoar',1.2),('EvolutionSmirk',1.)]
actions={}
reference=json.loads((OUT/'mesh_reference.json').read_text(encoding='utf-8-sig'))['bones']
ue_rest={b['name']:Matrix.LocRotScale(Vector(b['t']),Quaternion((b['q'][3],*b['q'][:3])),Vector((1,1,1))) for b in reference}
reflection=Matrix.Diagonal((1.,-1.,1.,1.))
performance={}
for name,seconds in clips:
    rig.animation_data_clear()
    action=bpy.data.actions.new('AS_Lizard_'+name);action.use_fake_user=True
    rig.animation_data_create().action=action
    frames=round(seconds*60)
    baked=[]
    scene.frame_start=0;scene.frame_end=frames
    for f in range(frames+1):
        scene.frame_set(f);t=f/60.;u=f/frames
        if name=='EvolutionRise':pose(coil=ease(u),t=t)
        elif name=='EvolutionHover':pose(coil=1.,t=t)
        elif name=='ArmorAssembly':pose(coil=1.,assembly=ease(t/.32),t=t)
        elif name=='ArmorAttach':pose(coil=1.,assembly=1.,reaction=math.sin(u*math.pi)*3.,t=0.)
        elif name=='HeroicLand':pose(coil=1.-ease(u),assembly=1.,settle=math.sin(ease((u-.72)/.28)*math.pi),t=0.)
        elif name=='EvolutionRoar':pose(assembly=1.-ease(u/.28),roar=ease(u/.27)*(1.-.35*ease((u-.65)/.35)),t=t)
        else:pose(roar=.65*(1.-ease(u/.45)),smile=ease(u/.5),t=t)
        for pb in rig.pose.bones:
            pb.rotation_mode='QUATERNION'
            pb.keyframe_insert('location',frame=f,group=pb.name)
            pb.keyframe_insert('rotation_quaternion',frame=f,group=pb.name)
            pb.keyframe_insert('scale',frame=f,group=pb.name)
        # Convert skinning deltas through each mesh's true bind pose. Never use
        # the shared human skeleton's reference lengths or frozen parent frames.
        worlds={}
        local=[]
        for b in reference:
            n=b['name'];p=rig.pose.bones.get(n)
            world=(reflection @ p.matrix @ rest[n].inverted() @ reflection @ ue_rest[n]) if p else ue_rest[n]
            worlds[n]=world
            m=worlds[b['parent']].inverted() @ world if b['parent'] else world
            tr,q,sc=m.decompose()
            local.append([*tr,q.x,q.y,q.z,q.w,*sc])
        baked.append(local)
    actions[name]=action
    performance[name]=dict(seconds=frames/60.,frames=baked)
    print('BAKED',name,frames+1)
(OUT/'performance.json').write_text(json.dumps(dict(bones=[b['name'] for b in reference],clips=performance)))

def select_export():
    bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);body.select_set(True)
    bpy.context.view_layer.objects.active=rig
def export(path,animation):
    select_export()
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'ARMATURE','MESH'},
        add_leaf_bones=False,use_armature_deform_only=True,bake_anim=animation,bake_anim_use_all_actions=False,
        bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0.0,mesh_smooth_type='FACE',
        axis_forward='-Y',axis_up='Z',path_mode='AUTO')

rig.animation_data.action=None;reset()
export(OUT/'LizardEvolutionBody.fbx',False)
for name,seconds in clips:
    rig.animation_data.action=actions[name]
    scene.frame_start=0;scene.frame_end=round(seconds*60);scene.frame_set(0)
    export(OUT/(name+'.fbx'),True)

# Keep source actions and named pose markers available for direct editing.
rig.animation_data.action=actions['EvolutionRise'];scene.frame_end=75;scene.frame_set(75)
for label,frame in [('Neutral',0),('Coil',75)]:scene.timeline_markers.new(label,frame=frame)
scene.render.engine='BLENDER_WORKBENCH'
scene.display.shading.light='STUDIO';scene.display.shading.color_type='MATERIAL'
scene.display.shading.show_cavity=True;scene.display.shading.cavity_type='BOTH'
scene.display.shading.background_type='WORLD'
scene.world=bpy.data.worlds.new('ReviewWorld');scene.world.color=(.07,.08,.10)
for m in bpy.data.materials:m.diffuse_color=(.42,.34,.26,1) if 'Eye' not in m.name else (1.,.2,.025,1)
bpy.ops.object.camera_add();cam=bpy.context.object;cam.name='ReviewCamera';scene.camera=cam
scene.render.resolution_x=960;scene.render.resolution_y=960;scene.render.resolution_percentage=100
cam.data.type='ORTHO'
for name,frame in [('EvolutionRise',0),('EvolutionRise',75),('ArmorAssembly',60),('EvolutionRoar',30),('EvolutionSmirk',60)]:
    rig.animation_data.action=actions[name];scene.frame_set(frame)
    smirk.value=.85 if name=='EvolutionSmirk' else 0.;narrow.value=.7 if name=='EvolutionSmirk' else 0.
    for angle,loc in [('front',(0,-4,1.15)),('side',(4,-.1,1.15)),('back',(0,4,1.15))]:
        cam.location=loc;target=Vector((0,0,1.));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=2.3
        scene.render.filepath=str(OUT/(name+'_'+str(frame)+'_'+angle+'.png'));bpy.ops.render.render(write_still=True)
    if name in ['EvolutionRoar','EvolutionSmirk'] or frame==0:
        cam.location=(0,-3,1.73);cam.rotation_euler=(Vector((0,-.1,1.7))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=.68
        scene.render.filepath=str(OUT/(name+'_face.png'));bpy.ops.render.render(write_still=True)
smirk.value=narrow.value=0.
rig.animation_data.action=actions['EvolutionRise'];scene.frame_set(75)
bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE/'LizardTransformation.blend'))
print('LIZARD_PERFORMANCE_BAKED')
