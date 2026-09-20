"""Editable coastal festival geometry for production brief facilities 32--70.

Reference interpretation: large silhouettes from Design/Facitilies, with the
brief taking precedence for open photo frames, safe footbath access, and flat
Road/Tile surfaces. Coordinates are metres, front +Y (floor travel +X).
"""
from math import cos, sin, pi


def _prism(b, name, profile, y, depth, mat, collision=False):
    """Closed extrusion of an x/z polygon. Suitable for shell/roof/arch pieces."""
    signed_area=sum(a[0]*c[1]-c[0]*a[1] for a,c in zip(profile,profile[1:]+profile[:1]))
    if signed_area>0:
        profile=list(reversed(profile))
    count = len(profile)
    verts = [(x, y - depth / 2, z) for x, z in profile]
    verts += [(x, y + depth / 2, z) for x, z in profile]
    faces = [tuple(reversed(range(count))), tuple(range(count, count * 2))]
    faces += [(i, (i + 1) % count, (i + 1) % count + count, i + count)
              for i in range(count)]
    return b.mesh(name, verts, faces, mat, collision=collision)


def _sheet(b, name, rows, mat, thickness=0.035):
    """A closed, gently curved fabric panel, with visible back and edge faces."""
    nr, nc = len(rows), len(rows[0])
    verts = [tuple(p) for row in rows for p in row]
    normal=[0.0,0.0,0.0]
    for j in range(nr-1):
        for i in range(nc-1):
            p=rows[j][i]
            u=[rows[j][i+1][k]-p[k] for k in range(3)]
            v=[rows[j+1][i][k]-p[k] for k in range(3)]
            cross=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
            normal=[normal[k]+cross[k] for k in range(3)]
    magnitude=sum(v*v for v in normal)**.5
    normal=[v/max(magnitude,1e-10) for v in normal] if magnitude>1e-10 else [0,0,1]
    verts += [tuple(p[k]-normal[k]*thickness for k in range(3)) for p in verts]
    n = nr * nc
    faces = []
    for j in range(nr - 1):
        for i in range(nc - 1):
            a = j * nc + i
            faces += [(a, a + 1, a + nc + 1, a + nc),
                      (a + n + nc, a + n + nc + 1, a + n + 1, a + n)]
    edge = list(range(nc)) + [j * nc + nc - 1 for j in range(1, nr)]
    edge += list(range((nr - 1) * nc + nc - 2, (nr - 1) * nc - 1, -1))
    edge += [j * nc for j in range(nr - 2, 0, -1)]
    faces += [(c, a, a + n, c + n) for a, c in zip(edge, edge[1:] + edge[:1])]
    return b.mesh(name, verts, faces, mat)


def _cone(b, name, x, y, z, radius, height, mat, sides=20):
    verts = [(x + radius * cos(2*pi*i/sides), y + radius * sin(2*pi*i/sides), z)
             for i in range(sides)] + [(x, y, z + height)]
    faces = [tuple(reversed(range(sides)))]
    faces += [(i, (i + 1) % sides, sides) for i in range(sides)]
    return b.mesh(name, verts, faces, mat)


def _ring(b, name, x, y, z, outer, inner, height, mat, start=0, end=2*pi, steps=28):
    """Closed annular sector, leaving the central footbath/seat opening empty."""
    verts = []
    for i in range(steps + 1):
        a = start + (end - start) * i / steps
        for r, zz in [(outer, z), (inner, z), (outer, z + height), (inner, z + height)]:
            verts.append((x + r * cos(a), y + r * sin(a), zz))
    faces = [(0, 1, 3, 2), (steps*4+2, steps*4+3, steps*4+1, steps*4)]
    for i in range(steps):
        k = i * 4
        faces += [(k, k+4, k+5, k+1), (k+2, k+3, k+7, k+6),
                  (k, k+2, k+6, k+4), (k+1, k+5, k+7, k+3)]
    return b.mesh(name, verts, [tuple(reversed(face)) for face in faces], mat)


def _plant(b, name, x, y, z=0, size=0.6):
    b.box(name + '_Planter', (x, y, z + size*.3), (size, size, size*.6), 'Cream', collision=True)
    for i in range(7):
        a = i * 2.399
        b.sphere(name + '_Leaf', (x + cos(a)*size*.26, y + sin(a)*size*.26,
                                  z + size*(.7 + .05*(i % 3))),
                 (size*.23, size*.2, size*.24), 'Green')


def _lantern(b, name, loc, scale=1.0, mat='Teal', physical=True):
    x, y, z = loc
    r = .15 * scale
    b.cyl(name + '_Lower', (x, y, z - .20*scale), r*1.3, .06*scale, mat, vertices=12)
    b.cyl(name + '_Glow', (x, y, z), r, .35*scale, 'Amber', vertices=12)
    for i in range(4):
        a = pi/4 + i*pi/2
        b.beam(name + '_Rib', (x+r*cos(a), y+r*sin(a), z-.18*scale),
               (x+r*cos(a), y+r*sin(a), z+.18*scale), .016*scale, mat)
    _cone(b, name + '_Cap', x, y, z + .18*scale, r*1.45, .15*scale, mat, 12)
    b.torus(name + '_Ring', (x,y,z+.40*scale), .07*scale, .012*scale, mat)
    if physical:
        b.guide('Light_' + name, (x, y, z))


def _bench(b, name, x, y, width=1.8, z=0, back=True):
    for i in range(3):
        b.box(name + '_SeatSlat', (x, y + (i-1)*.17, z+.46), (width, .155, .09), 'Wood')
    for xx in [x-width*.38, x+width*.38]:
        b.box(name + '_Support', (xx, y, z+.23), (.12, .5, .46), 'Teal', collision=True)
    if back:
        for zz in [.66, .83, 1.0]:
            b.box(name + '_BackSlat', (x,y-.25,z+zz), (width,.08,.15), 'Wood')
        for xx in [x-width*.44,x+width*.44]:
            b.box(name + '_BackUpright', (xx,y-.25,z+.76), (.1,.12,.65), 'Teal')
            b.box(name + '_Arm', (xx,y,z+.70), (.12,.58,.09), 'Wood')
    b.guide(name + '_SeatUse', (x,y+.18,z+.48))


def _deck(b, name, w, d, z=.15, mat='Wood'):
    b.box(name + '_Base', (0,0,z/2), (w,d,z), 'DarkTeal', collision=True)
    for i in range(max(5, int(d/.35))):
        n = max(5,int(d/.35)); dy=d/n
        b.box(name + '_Board', (0,-d/2+(i+.5)*dy,z+.025), (w-.04,dy-.018,.05), mat, bevel=.008)


def _steps(b, name, x, y, width, top, count=3):
    for i in range(count):
        hh = top*(count-i)/count
        b.box(name + '_Step', (x,y+(i+.5)*.25,hh/2), (width,.27,hh), 'Wood', collision=True)


def _flat_canopy(b, name, w, d, h, stripe=True):
    for x in [-w*.44,w*.44]:
        for y in [-d*.40,d*.40]:
            b.box(name + '_Post', (x,y,h*.48), (.14,.14,h*.96), 'Teal', collision=True)
    n=8 if stripe else 1
    for i in range(n):
        b.box(name + '_Canopy', (-w/2+w*(i+.5)/n,0,h-.08), (w/n,d,.16),
              'Teal' if stripe and i%2==0 else 'Cream', bevel=.015)
    b.box(name + '_FrontValance', (0,d/2-.035,h-.18), (w,.09,.23), 'Cream')


def _sail_roof(b, name, w, d, h):
    for x in [-w*.44,w*.44]:
        for y in [-d*.43,d*.43]:
            b.box(name + '_Mast', (x,y,h*.49), (.14,.14,h*.98), 'Wood', collision=True)
    for side in [-1,1]:
        rows=[]
        for j in range(9):
            t=j/8
            rows.append([(side*w*.46*t, yy*d*.46, h-.04-.28*sin(pi*t)) for yy in [-1,0,1]])
        _sheet(b,name+'_TensionFabric',rows,'Teal' if side==1 else 'Cream')


def _parasol(b, name, x, y, z, radius, height):
    b.cyl(name + '_Pole', (x,y,z+height*.47), .06, height*.94, 'Wood', collision=True)
    for i in range(8):
        a,aa=i*pi/4,(i+1)*pi/4
        rows=[[(x,y,z+height),(x+radius*.55*cos(a),y+radius*.55*sin(a),z+height-.23)],
              [(x+radius*cos(aa),y+radius*sin(aa),z+height-.55),
               (x+radius*cos(a),y+radius*sin(a),z+height-.55)]]
        _sheet(b,name+'_UmbrellaPanel',rows,'Teal' if i%2 else 'Cream')


def _speaker(b, name, x, y, z, height=.95):
    b.box(name + '_Cabinet', (x,y,z+height*.5), (height*.5,height*.42,height), 'DarkTeal', collision=True)
    for zz,rr in [(.3,.15),(.73,.10)]:
        b.cyl(name + '_Driver', (x,y+height*.215,z+height*zz), height*rr,.025,'Metal',
              rotation=(pi/2,0,0))
        b.sphere(name + '_Dome', (x,y+height*.24,z+height*zz), (height*rr*.5,.025,height*rr*.5), 'DarkTeal')


def _mic(b, name, x, y, z, h=1.3):
    for a in [0,2*pi/3,4*pi/3]:
        b.beam(name+'_Foot',(x,y,z+.12),(x+.30*cos(a),y+.30*sin(a),z+.035),.018,'Metal')
    b.beam(name+'_Stand',(x,y,z+.10),(x,y,z+h),.022,'Metal')
    b.beam(name+'_Boom',(x,y,z+h),(x+.25,y+.20,z+h+.05),.02,'Metal')
    b.cyl(name+'_Microphone',(x+.25,y+.23,z+h+.05),.045,.15,'DarkTeal',rotation=(pi/2,0,0))
    b.guide(name+'_AudioAttachment',(x+.25,y+.25,z+h+.05))


def _curtains(b, name, w, y, bottom, top, color='Teal'):
    for s in [-1,1]:
        for i in range(6):
            x=s*(w*.37+i*w*.018)
            b.cyl(name + '_GatheredFold', (x,y+.025*sin(i), (top+bottom)/2),
                  w*.035,top-bottom,color,vertices=10)
        b.box(name+'_Tie',(s*w*.415,y+.12,bottom+(top-bottom)*.38),(w*.15,.10,.12),'Amber')
        b.guide(name+'_CurtainTrack',(s*w*.42,y,top))


def _banner(b, name, label, x,y,z,w,h=.4, mat='Teal'):
    b.box(name+'_Panel',(x,y,z),(w,.07,h),mat)
    b.text(name+'_Legend',label,(x,y+.045,z),min(h*.48,w/max(4,len(label))*.85),
           'DarkTeal' if mat=='Cream' else 'Cream')


def _rest(b, e, w,d,h):
    n=e['number']
    if n==32:
        _flat_canopy(b,'Hammock',w*.96,d*.88,h)
        rows=[]
        for j in range(5):
            yy=(j/4-.5)*d*.38
            rows.append([(-w*.37+i*w*.74/16,yy,h*.34+h*.21*(2*i/16-1)**2) for i in range(17)])
        _sheet(b,'Hammock_CreamFabric',rows,'Cream',.04)
        for yy in [-d*.13,0,d*.13]:
            b.curve('Hammock_TealStripe',[(-w*.37+i*w*.74/16,yy,h*.347+h*.21*(2*i/16-1)**2) for i in range(17)],.045,'Teal')
        for s in [-1,1]:
            b.beam('Hammock_Spreader',(s*w*.37,-d*.20,h*.55),(s*w*.37,d*.20,h*.55),.035,'Wood')
            for yy in [-d*.20,0,d*.20]:
                b.beam('Hammock_Rope',(s*w*.37,yy,h*.55),(s*w*.43,0,h*.73),.018,'Cream')
        _plant(b,'HammockPlant',-w*.38,-d*.31,size=min(w,d)*.14)
        b.box('SideTable',(w*.35,d*.22,.3),(.45,.45,.6),'Wood',collision=True)
        b.guide('Hammock_SeatUse',(0,0,h*.36))
    elif n==33:
        _bench(b,'CoastalBench',0,0,w*.94)
        b.guide('ViewDirection_PositiveY',(0,d*.45,1))
    elif n==34:
        b.cyl('SailMast',(0,0,h*.48),.11,h*.96,'Cream',collision=True)
        for i in range(4):
            a=i*pi/2+pi/4
            p1=(cos(a)*w*.46,sin(a)*d*.46,h*(.74 if i%2 else .88))
            p2=(cos(a+pi/2)*w*.46,sin(a+pi/2)*d*.46,h*(.88 if i%2 else .74))
            _sheet(b,'TriangularSail',[[(0,0,h*.93),p1],[(0,0,h*.77),p2]],'Teal' if i%2 else 'Cream')
            b.beam('SailEdge',(0,0,h*.94),p1,.028,'Cream')
        r=min(w,d)*.27
        for i in range(3):
            a=i*2*pi/3
            _ring(b,'CircularSeat',0,0,.39,r,r-.35,.12,'Wood',a+.12,a+1.7,10)
            for aa in [a+.35,a+1.4]:
                b.box('SeatFoot',(cos(aa)*(r-.15),sin(aa)*(r-.15),.20),(.18,.18,.40),'Teal',collision=True)
        b.guide('Shade_SeatUse',(0,r-.2,.53))
    elif n==35:
        # An open slatted pergola, not a solid marquee roof beneath the leaves.
        for x in [-w*.44,w*.44]:
            for y in [-d*.40,d*.40]:
                b.box('PergolaPost',(x,y,h*.49),(.15,.15,h*.98),'Teal',collision=True)
        for s in [-1,1]:
            b.box('PergolaEdgeBeamX',(0,s*d*.40,h-.05),(w*.96,.15,.18),'Wood')
            b.box('PergolaEdgeBeamY',(s*w*.44,0,h-.05),(.15,d*.94,.18),'Wood')
        for i in range(11):
            x=-w*.45+i*w*.09
            b.box('Pergola_Rafter',(x,0,h+.03),(.11,d*.98,.13),'Wood')
            for j in range(3):
                yy=-d*.40+j*d*.4
                if (i+j)%3!=1:
                    b.sphere('CanopyFoliage',(x,yy,h+.13),(.33,.40,.14),'Green')
                    b.sphere('TrailingCanopyFoliage',(x+.12,yy+.24,h+.11),(.22,.32,.11),'Green')
        for x in [-w*.44,w*.44]:
            for y in [-d*.40,d*.40]:
                _plant(b,'ClimbingVine',x,y,size=.5)
                for j in range(6):
                    b.sphere('VineLeaf',(x+.07*sin(j*1.7),y+.07*cos(j),h*(j+.7)/7),(.15,.17,.19),'Green')
        _bench(b,'PergolaBenchLeft',-w*.24,-d*.20,w*.33)
        _bench(b,'PergolaBenchRight',w*.24,-d*.20,w*.33)
        b.table('PergolaTable',0,d*.02,width=w*.27,depth=d*.2)
        _lantern(b,'PergolaLamp',(-w*.39,d*.4,h*.70),.8)
    elif n==36:
        _flat_canopy(b,'PicnicCanopy',w*.96,d*.92,h)
        for i in range(8):
            for j in range(6):
                b.box('PicnicBlanketCheck',(-w*.35+(i+.5)*w*.70/8,-d*.34+(j+.5)*d*.68/6,.035),
                      (w*.70/8,d*.68/6,.035),'Teal' if (i+j)%2 else 'Cream',bevel=.004)
        b.box('LowPicnicTable',(0,0,.30),(w*.4,d*.28,.12),'Wood',collision=True)
        for x in [-w*.15,w*.15]:
            b.box('TableLeg',(x,0,.15),(.10,d*.22,.30),'Wood')
        b.box('Basket',(-w*.1,-d*.1,.59),(.45,.35,.46),'Wood')
        b.torus('BasketHandle',(-w*.1,-d*.1,.85),.20,.025,'DarkWood')
        for s in [-1,1]:
            b.box('PicnicCushion',(s*w*.25,d*.2,.09),(.55,.42,.14),'Teal',bevel=.08)
            b.sphere('Shoes',(s*.13,d*.43,.07),(.10,.19,.07),'Cream')
            b.guide('Picnic_SeatUse',(s*w*.25,d*.2,.15))
        b.cyl('Thermos',(w*.13,0,.56),.09,.4,'Metal')
    elif n==37:
        for i in range(5):
            b.box('TableTopPlank',(0,(i-2)*d*.10,.80),(w*.96,d*.094,.12),'Wood')
        for s in [-1,1]:
            b.box('LongBench',(0,s*d*.38,.45),(w*.96,d*.19,.12),'Wood',collision=True)
            for x in [-w*.33,w*.33]:
                b.beam('AFrameLeg',(x,s*d*.39,.04),(x,s*d*.13,.79),.065,'Teal',collision=True)
                b.box('BenchCrossbeam',(x,0,.39),(.14,d*.85,.12),'Teal')
            b.guide('GroupPicnic_SeatUse',(0,s*d*.37,.50))
    elif n==38:
        _flat_canopy(b,'WaterLounge',w*.96,d*.92,h,False)
        _banner(b,'WaterHeader','WATER REFILL',0,d*.46,h-.25,w*.88,.32)
        for i in range(3):
            x=(i-1)*w*.20
            hh=1.28 if i==1 else .95
            b.box('RefillStation',(x,-d*.05,hh*.5),(.5,.5,hh),'Cream',collision=True)
            b.box('BottleRecess',(x,d*.207,hh*.65),(.31,.02,.4),'DarkTeal')
            b.box('BottleTray',(x,d*.20,hh*.40),(.38,.25,.045),'Metal')
            b.beam('Tap',(x,d*.13,hh*.91),(x,d*.31,hh*.91),.025,'Metal')
            b.cyl('Bottle',(x,d*.27,hh*.59),.065,.24,'Blue')
            b.guide('WaterEffect',(x,d*.31,hh*.87))
        _bench(b,'WaterBench',0,d*.32,w*.38,back=False)
        _plant(b,'WaterPlanterL',-w*.39,d*.28,size=.55)
        _plant(b,'WaterPlanterR',w*.39,d*.28,size=.55)
    elif n==39:
        for x in [-w*.40,w*.40]:
            for y in [-d*.40,d*.40]:
                b.box('FootbathPost',(x,y,h*.39),(.17,.17,h*.78),'Wood',collision=True)
        verts=[(-w*.49,-d*.49,h*.75),(w*.49,-d*.49,h*.75),(w*.49,d*.49,h*.75),(-w*.49,d*.49,h*.75),(0,0,h)]
        b.mesh('ThatchedPavilionRoof',verts,[(0,4,1),(1,4,2),(2,4,3),(3,4,0),(3,2,1,0)],'Wood')
        for i in range(1,6):
            t=i/6
            z=h-(h*.25*t)
            for s in [-1,1]:
                b.beam('ThatchTier',(-w*.49*t,s*d*.49*t,z),(w*.49*t,s*d*.49*t,z),.045,'Sand')
                b.beam('ThatchTier',(s*w*.49*t,-d*.49*t,z),(s*w*.49*t,d*.49*t,z),.045,'Sand')
        b.box('BasinBottom',(0,0,.07),(w*.74,d*.74,.14),'Cream',collision=True)
        b.box('IndependentWaterSurface',(0,0,.20),(w*.63,d*.63,.02),'Blue')
        for s in [-1,1]:
            b.box('BasinSideWall',(s*w*.36,0,.22),(.15,d*.74,.44),'Cream',collision=True)
            b.box('BasinEndWall',(0,s*d*.36,.22),(w*.59,.15,.44),'Cream',collision=True)
            b.box('FootbathSeatSide',(s*w*.36,0,.48),(.32,d*.76,.12),'Wood',collision=True)
            b.box('FootbathSeatEnd',(0,s*d*.36,.48),(w*.57,.32,.12),'Wood',collision=True)
        b.guide('WaterSurfaceRuntime',(0,0,.22))
        b.guide('Footbath_SeatUse',(0,d*.34,.54))


def _performance(b,e,w,d,h):
    n=e['number']; dz=.42 if n!=44 else .16
    _deck(b,'StageDeck',w*.94,d*.80,dz)
    if n!=44:
        _steps(b,'StageEntry',0,d*.40,w*.27,dz)
    if n==40:
        roof_z=h*.70  # Below the wave troughs; crossbars must not pierce the fabric.
        for s in [-1,1]:
            x=s*w*.42
            for yy in [-d*.20,d*.16]:
                b.beam('TrussVertical',(x,yy,dz),(x,yy,roof_z),.06,'Metal',collision=True)
            for j in range(6):
                z=dz+(roof_z-dz)*j/6
                b.beam('TrussDiagonal',(x,-d*.20,z),(x,d*.16,z+(roof_z-dz)/6),.028,'Metal')
            _speaker(b,'SpeakerTower',x,d*.19,dz,height=h*.26)
        for j in range(7):
            y=-d*.37+j*d*.74/6
            b.beam('RoofTruss',(-w*.43,y,roof_z),(w*.43,y,roof_z),.045,'Metal')
        for j in range(6):
            rows=[]
            for yy in [-d*.45+j*d*.15,-d*.45+(j+1)*d*.15]:
                rows.append([(-w*.50+i*w/32,yy,h*.87+h*.105*cos((i/32-.5)*3*pi)) for i in range(33)])
            _sheet(b,'WaveRoofPanel',rows,'Teal' if j%2 else 'Cream',.09)
        b.box('ScreenFrame',(0,-d*.30,h*.43),(w*.70,.16,h*.53),'DarkTeal',collision=True)
        b.box('IndependentVideoScreen',(0,-d*.208,h*.43),(w*.66,.025,h*.47),'Screen')
        b.cyl('ApronStage',(0,d*.27,dz*.7),w*.16,dz*1.4,'Teal',vertices=40,collision=True)
        for i in range(7):
            _lantern(b,'StageLight',(-w*.35+i*w*.7/6,d*.20,h*.75),.55)
        b.guide('VideoScreenRuntime',(0,-d*.20,h*.45))
    elif n==41:
        b.box('PuppetStageBack',(0,-d*.22,h*.46),(w*.80,.16,h*.77),'DarkTeal',collision=True)
        for s in [-1,1]:
            b.box('TheaterPillar',(s*w*.40,-d*.12,h*.45),(w*.12,d*.18,h*.87),'Cream',collision=True)
            b.box('PillarInset',(s*w*.40,d*.005,h*.48),(w*.07,.025,h*.64),'Teal')
        _banner(b,'TheaterHeader','PUPPET THEATRE',0,-d*.06,h*.89,w*.82,h*.15)
        _curtains(b,'PuppetCurtain',w*.83,-d*.08,h*.43,h*.79)
        b.box('PuppetCounter',(0,0,h*.35),(w*.64,.40,h*.18),'Wood',collision=True)
        for i in [-1,1]:
            x=i*w*.13
            b.sphere('PuppetHead',(x,0,h*.55),(.16,.13,.16),'Sand' if i<0 else 'Cream')
            b.sphere('PuppetBody',(x,-.02,h*.45),(.15,.10,.18),'Coral' if i<0 else 'Teal')
            for s in [-1,1]:
                b.sphere('PuppetEar',(x+s*.105,0,h*.60),(.07,.07,.13 if i>0 else .07),'Cream' if i>0 else 'Sand')
            b.guide('PuppetAnimation',(x,-.1,h*.48))
        for j in range(2):
            for i in [-1,1]:
                _bench(b,'TheaterAudience',i*w*.22,d*(.18+j*.17),w*.34,z=dz,back=False)
    elif n==42:
        for s in [-1,1]:
            b.beam('CinemaUpright',(s*w*.41,-d*.30,dz),(s*w*.41,-d*.30,h*.90),.08,'Metal',collision=True)
            _speaker(b,'CinemaSpeaker',s*w*.4,-d*.13,dz,.9)
        b.box('ScreenFrame',(0,-d*.29,h*.55),(w*.79,.18,h*.67),'DarkTeal',collision=True)
        b.box('IndependentFilmScreen',(0,-d*.19,h*.55),(w*.74,.025,h*.59),'Screen')
        for j in range(3):
            for i in range(4):
                b.box('FloorCushion',((i-1.5)*w*.16,d*(-.02+j*.16),dz+.10),(w*.12,d*.11,.15),'Teal' if (i+j)%2 else 'Cream',bevel=.04)
                b.guide('Cinema_SeatUse',((i-1.5)*w*.16,d*(-.02+j*.16),dz+.20))
        b.box('ProjectionBooth',(w*.37,d*.27,dz+.5),(w*.16,d*.22,1.0),'Teal',collision=True)
        b.box('Projector',(w*.37,d*.25,dz+1.09),(.45,.30,.25),'DarkTeal')
        b.cyl('ProjectorLens',(w*.37,d*.09,dz+1.09),.085,.10,'Glass',rotation=(pi/2,0,0))
        b.guide('MovieProjectionRuntime',(w*.37,d*.08,dz+1.09))
    elif n in [43,46]:
        _sail_roof(b,'PerformanceCanopy',w*.97,d*.86,h)
        _banner(b,'PerformanceBackdrop','LOCAL BAND' if n==43 else 'DANCE CHALLENGE',0,-d*.36,h*.56,w*.76,h*.45,'Cream')
        _banner(b,'FrontPerformanceHeader','LOCAL BAND' if n==43 else 'DANCE CHALLENGE',
                0,d*.425,h*.83,w*.67,h*.12,'Teal')
        for s in [-1,1]:
            _speaker(b,'PerformanceSpeaker',s*w*.40,0,dz,1.3)
            _plant(b,'StagePlant',s*w*.41,d*.24,dz,.52)
            for yy in [-d*.3,d*.28]:
                _lantern(b,'StageLantern',(s*w*.41,yy,h*.74),.60)
        if n==43:
            b.cyl('BassDrum',(0,-d*.14,dz+.39),.39,.42,'Teal',rotation=(pi/2,0,0))
            b.cyl('BassDrumSkin',(0,-d*.14+.22,dz+.39),.34,.025,'Cream',rotation=(pi/2,0,0))
            for s in [-1,1]:
                b.cyl('TomDrum',(s*.34,-d*.13,dz+.92),.19,.24,'Wood')
                b.beam('CymbalStand',(s*.65,-d*.2,dz),(s*.65,-d*.2,dz+1.1),.018,'Metal')
                b.cyl('Cymbal',(s*.65,-d*.2,dz+1.12),.28,.025,'Amber')
            _mic(b,'LeadMic',-w*.22,d*.13,dz)
            _mic(b,'SecondMic',w*.22,d*.13,dz)
            b.guide('InstrumentAnimation',(0,-d*.14,dz+.8))
        else:
            for i in range(5):
                for j in range(4):
                    b.box('IndependentDancePad',((i-2)*w*.13,(j-1.5)*d*.13,dz+.065),
                          (w*.122,d*.122,.022),'Amber' if (i+j)%3==0 else 'Teal' if (i+j)%2 else 'Cream')
            b.box('HostPodium',(w*.27,-d*.16,dz+.55),(.7,.6,1.1),'Teal',collision=True)
            b.guide('DanceScoreRuntime',(0,0,dz+.1))
    elif n==44:
        _parasol(b,'BuskerParasol',-w*.12,-d*.18,dz,min(w,d)*.44,h-dz)
        _mic(b,'BuskerMic',0,d*.10,dz,h=min(1.4,h*.55))
        _speaker(b,'BuskerSpeaker',-w*.34,d*.04,dz,.8)
        profile=[(-.64,.12),(-.78,.25),(-.74,.43),(-.55,.50),(-.24,.40),(.13,.34),(.49,.30),(.55,.21),(.48,.12),(-.25,.12)]
        _prism(b,'GuitarCase',[(x+w*.24,z+dz) for x,z in profile],-d*.14,.35,'DarkTeal')
    elif n==45:
        b.box('MagicBackdrop',(0,-d*.31,h*.48),(w*.87,.16,h*.83),'DarkTeal',collision=True)
        _curtains(b,'MagicCurtain',w*.86,-d*.20,dz,h*.84)
        _banner(b,'MagicHeader','MAGIC',0,-d*.18,h*.92,w*.63,h*.15)
        b.cyl('MagicTableStem',(0,0,dz+.45),.08,.85,'Wood')
        b.cyl('MagicTableCloth',(0,0,dz+.90),w*.14,.10,'Teal',vertices=24,collision=True)
        b.cyl('TopHatBrim',(0,0,dz+1.00),.28,.035,'DarkTeal')
        b.cyl('TopHatCrown',(0,0,dz+1.17),.19,.32,'DarkTeal')
        b.cyl('TopHatRibbon',(0,0,dz+1.08),.196,.07,'Amber')
        for s in [-1,1]:
            b.box('PropChest',(s*w*.29,0,dz+.25),(.62,.50,.50),'Wood',collision=True)
            b.box('ChestBand',(s*w*.29,.26,dz+.25),(.09,.04,.48),'Amber')
        b.guide('MagicPropAnimation',(0,0,dz+1.0))
    b.guide('PerformerPosition',(0,d*.12,dz+.08))
    b.guide('AudienceDirection_PositiveY',(0,d*.53,.05))


def _safety(b,e,w,d,h):
    n=e['number']
    if n==47:
        b.box('MedicalCartBody',(0,0,h*.33),(w*.80,d*.65,h*.38),'Cream',collision=True)
        b.box('MedicalCartSkirt',(0,0,h*.16),(w*.81,d*.66,h*.12),'Teal')
        b.box('MedicalCounter',(0,0,h*.54),(w*.92,d*.78,.11),'Wood')
        for s in [-1,1]:
            b.cyl('FixedCartWheel',(s*w*.30,d*.34,h*.14),h*.14,.12,'DarkTeal',rotation=(pi/2,0,0))
            b.cyl('WheelHub',(s*w*.30,d*.405,h*.14),h*.075,.018,'Metal',rotation=(pi/2,0,0))
            b.beam('CanopySupport',(s*w*.39,0,h*.54),(s*w*.39,0,h*.915),.04,'Metal')
        for i in range(6):
            x=-w*.48+(i+.5)*w*.96/6
            _prism(b,'CartAwning',[(x-w*.08,h*.91),(x+w*.08,h*.91),(x+w*.08,h*.97),(x-w*.08,h*.97)],0,d*.92,'Teal' if i%2 else 'Cream')
        _banner(b,'FirstAidSign','FIRST AID',0,d*.333,h*.35,w*.68,h*.16,'Teal')
        b.box('MedicalBag',(0,0,h*.64),(.56,.3,.36),'Cream')
        b.box('BagCrossVertical',(0,.16,h*.64),(.07,.025,.22),'Teal')
        b.box('BagCrossHorizontal',(0,.16,h*.64),(.22,.025,.07),'Teal')
        b.torus('BagHandle',(0,0,h*.86),.1,.024,'DarkTeal')
        for s in [-1,1]:
            b.cyl('SupplyBottle',(s*w*.23,0,h*.66),.07,.3,'White')
        b.guide('CartFixedWheel_NoVehicleRuntime',(w*.30,d*.34,h*.14))
    elif n==48:
        b.box('SecurityCabinBase',(0,0,.10),(w*.84,d*.83,.2),'Cream',collision=True)
        b.box('SecurityCabinLower',(0,0,h*.25),(w*.77,d*.76,h*.43),'Teal',collision=True)
        for x in [-w*.36,w*.36]:
            for y in [-d*.35,d*.35]:
                b.box('WindowPost',(x,y,h*.64),(.09,.09,h*.55),'Cream',collision=True)
        b.box('FrontObservationGlass',(-w*.1,d*.35,h*.63),(w*.52,.025,h*.36),'Glass')
        b.box('SideObservationGlass',(-w*.36,0,h*.63),(.025,d*.64,h*.36),'Glass')
        b.box('CabinRoof',(0,0,h*.90),(w*.94,d*.96,.18),'Teal')
        b.box('RoofVent',(0,0,h*.97),(w*.26,d*.28,.14),'Cream')
        _banner(b,'SecurityHeader','SECURITY',0,d*.48,h*.86,w*.86,.25)
        b.box('Radio',(-w*.13,d*.15,h*.46),(.13,.11,.22),'DarkTeal')
        b.beam('RadioAntenna',(-w*.13,d*.15,h*.57),(-w*.13,d*.15,h*.73),.012,'Metal')
        _plant(b,'WindowPlanter',-w*.16,d*.42,size=w*.30)
        b.guide('CabinEntrance',(w*.41,0,.05))
    elif n==49:
        for x in [-w*.43,w*.43]:
            b.box('EquipmentPost',(x,0,h*.48),(.13,.15,h*.96),'Teal',collision=True)
        b.box('EquipmentBack',(0,-d*.15,h*.34),(w*.80,.12,h*.57),'Cream',collision=True)
        b.box('EquipmentCounter',(0,0,h*.62),(w*.91,d*.65,.09),'Wood')
        _banner(b,'FireResponseHeader','FIRE RESPONSE',0,0,h*.85,w*.83,h*.26)
        for i in range(5):
            b.torus('HoseReel',(-w*.24,.0+i*.024,h*.34),h*.20,.032,'Coral')
        b.cyl('HoseAxle',(-w*.24,.07,h*.34),.08,.35,'Metal',rotation=(pi/2,0,0))
        b.cyl('Extinguisher',(w*.06,d*.1,h*.27),w*.065,h*.38,'Coral',collision=True)
        b.box('ExtinguisherHandle',(w*.06,d*.1,h*.48),(.18,.10,.06),'Metal')
        b.curve('ExtinguisherHose',[(w*.06,d*.1,h*.47),(w*.16,d*.1,h*.43),(w*.17,d*.1,h*.20)],.025,'DarkTeal')
        b.box('EmergencyKit',(w*.29,0,h*.36),(w*.20,d*.46,h*.22),'Cream',collision=True)
        b.box('KitCrossA',(w*.29,d*.24,h*.36),(.055,.025,.19),'Teal')
        b.box('KitCrossB',(w*.29,d*.24,h*.36),(.19,.025,.055),'Teal')
        b.guide('EquipmentUse',(0,d*.43,0))
    elif n==50:
        # Ground stays empty and accessible; sign collision is behind the gathering area.
        b.box('AssemblyGround',(0,0,.025),(w,d,.05),'Cream',collision=True)
        for s in [-1,1]:
            b.box('AssemblySignPost',(s*w*.38,-d*.37,h*.46),(.13,.13,h*.92),'Teal',collision=True)
        _banner(b,'AssemblySign','ASSEMBLY POINT',0,-d*.37,h*.85,w*.90,h*.25)
        for sx in [-1,1]:
            for sy in [-1,1]:
                x=sx*w*.23;y=sy*d*.22
                b.box('GroundArrowShaft',(x,y,.061),(.08,d*.11,.008),'Teal',rotation=(0,0,-sx*sy*pi/4))
        for xx in [-.30,0,.30]:
            b.cyl('PersonHeadMark',(xx,0,.063),.09,.007,'Teal')
            b.box('PersonBodyMark',(xx,-.24,.063),(.15,.28,.007),'Teal')
        b.guide('AssemblyUse',(0,0,.08))


def _photo(b,e,w,d,h):
    n=e['number']
    if n==51:
        b.box('SandCastleBase',(0,0,.12),(w*.97,d*.92,.24),'Sand',bevel=.15,collision=True)
        for x, y, r, th in [(0,-d*.18,w*.18,h*.79),(-w*.31,0,w*.135,h*.54),(w*.31,0,w*.135,h*.57)]:
            b.cyl('SandTower',(x,y,th*.38),r,th*.76,'Sand',vertices=24,collision=True)
            _cone(b,'SandSpire',x,y,th*.76,r*.9,th*.25,'Sand')
            b.cyl('BattlementRim',(x,y,th*.72),r*1.1,.16,'Sand',vertices=24)
            for i in range(9):
                a=i*2*pi/9
                b.box('BattlementTooth',(x+r*cos(a),y+r*sin(a),th*.81),(.13,.13,.20),'Sand')
            b.beam('CastleFlagpole',(x,y,th),(x,y,th+.24),.018,'Wood')
            b.mesh('CastleFlag',[(x,y,th+.24),(x+.32,y,th+.13),(x,y,th+.04)],[(0,1,2)],'Teal')
        # The arched door is modeled as an actual opening between two jambs.
        for s in [-1,1]:
            b.box('GateJamb',(s*w*.13,d*.19,h*.18),(w*.11,.32,h*.35),'Sand',collision=True)
        radius=w*.16
        profile=[(radius*cos(a),h*.26+radius*sin(a)) for a in [i*pi/20 for i in range(21)]]
        profile += [(radius*.65*cos(a),h*.26+radius*.65*sin(a)) for a in [pi-i*pi/20 for i in range(21)]]
        _prism(b,'SandGateArch',profile,d*.19,.33,'Sand')
        _steps(b,'CastleSteps',0,d*.29,w*.25,h*.19,4)
        for s in [-1,1]:
            b.sphere('Dune',(s*w*.37,d*.30,.20),(w*.12,d*.13,.24),'Sand')
        b.guide('CastlePhotoPosition',(0,d*.53,.05))
    elif n==52:
        b.cyl('WhalePlinth',(0,0,.20),min(w,d)*.28,.4,'Wood',vertices=36,collision=True)
        _ring(b,'PlinthTealTrim',0,0,.36,min(w,d)*.285,min(w,d)*.24,.11,'Teal')
        # A closed broad fluke with a narrow rising peduncle, sculpted low-poly facets.
        profile=[(-w*.06,.40),(-w*.095,h*.43),(-w*.21,h*.61),(-w*.37,h*.69),(-w*.46,h*.89),
                 (-w*.46,h),(-w*.36,h*.85),(-w*.18,h*.82),(0,h*.70),(w*.18,h*.82),
                 (w*.36,h*.85),(w*.46,h),(w*.46,h*.89),(w*.37,h*.69),(w*.21,h*.61),
                 (w*.095,h*.43),(w*.06,.40)]
        _prism(b,'WhaleTailSculpture',profile,0,d*.18,'Teal')
        for s in [-1,1]:
            inset=[(s*w*.105,d*.09+.002,h*.54),(s*w*.41,d*.09+.002,h*.85),
                   (s*w*.23,d*.09+.002,h*.65)]
            b.mesh('WhaleFlukeCreamInset',inset,[(0,1,2) if s>0 else (2,1,0)],'Cream')
            b.curve('WaterSplash',[(s*w*.10,0,.42),(s*w*.24,0,.75),(s*w*.29,0,.51)],.07,'Blue')
        b.guide('WhalePhotoPosition',(0,d*.45,.03))
    elif n==53:
        _deck(b,'LetterPlinth',w*.98,d*.80,.22)
        for s in [-1,1]:
            b.box('LetterArchPost',(s*w*.45,-d*.18,h*.38),(.2,.23,h*.75),'Cream',collision=True)
        pts=[(w*.45*cos(pi*i/40),-d*.18,h*.49+h*.46*sin(pi*i/40)) for i in range(41)]
        b.curve('LetteringArch',pts,.13,'Teal')
        b.box('LowerLetterPlinth',(0,d*.06,.28),(w*.86,d*.38,.12),'Cream',collision=True)
        b.box('UpperLetterShelf',(0,0,h*.43),(w*.90,d*.30,.12),'Wood',collision=True)
        upper=b.text('FESTA_Letters','FESTA',(0,d*.06,h*.43+.065),w*.18,'Cream')
        lower=b.text('FIESTA_Letters','FIESTA',(0,d*.06,.345),w*.18,'Teal')
        for letters in [upper,lower]:
            letters.data.align_y='BOTTOM_BASELINE'
            letters.data.extrude=min(.14,d*.05)
            letters.data.bevel_depth=.012
            letters.data.offset=.018
        for s in [-1,1]:
            _plant(b,'LetteringPlant',s*w*.40,d*.20,.20,min(w,d)*.16)
        b.guide('GroupPhotoPosition',(0,d*.52,.05))
    elif n==54:
        # Low ship hull walls around a flat deck with an open entry at front.
        _deck(b,'PirateDeck',w*.78,d*.64,.38)
        hull=[(-w*.49,.75),(-w*.37,.28),(w*.32,.22),(w*.48,.61),(w*.45,1.02),(w*.26,.73),(-w*.30,.77)]
        _prism(b,'RearShipHull',hull,-d*.33,.14,'DarkWood')
        b.box('RearHullStructure',(0,-d*.33,.46),(w*.75,.14,.48),'DarkWood',collision=True)
        for s in [-1,1]:
            profile=[(s*w*.12,.25),(s*w*.39,.29),(s*w*.48,.68),(s*w*.42,.85),(s*w*.12,.70)]
            _prism(b,'FrontHullOpenEntry',profile,d*.32,.14,'DarkWood')
            b.box('FrontHullStructure',(s*w*.26,d*.32,.45),(w*.27,.13,.40),'DarkWood',collision=True)
            for xx in [.22,.34]:
                b.torus('ShipPorthole',(s*w*xx,d*.405,.53),.10,.025,'Cream')
        b.cyl('ShipMast',(-w*.12,-d*.05,h*.48),.085,h*.96,'Wood',collision=True)
        b.beam('SailYard',(-w*.39,-d*.04,h*.81),(w*.18,-d*.04,h*.81),.055,'Wood')
        rows=[]
        for j in range(10):
            t=j/9
            rows.append([(-w*.38+i*w*.52/12,-d*.04+.14*sin(pi*i/12)*sin(pi*t),h*(.40+.40*t)) for i in range(13)])
        _sheet(b,'CreamSail',rows,'Cream')
        b.sphere('SailSkull',(-w*.12,d*.09,h*.62),(.18,.035,.20),'DarkTeal')
        for sx in [-1,1]:
            b.beam('SailCrossbones',(-w*.12+sx*.19,d*.09,h*.49),(-w*.12-sx*.19,d*.09,h*.57),.03,'DarkTeal')
        b.beam('WheelStand',(w*.25,-d*.03,.42),(w*.25,-d*.03,1.16),.045,'Teal')
        b.torus('ShipWheel',(w*.25,0,1.23),.29,.045,'Wood')
        for i in range(8):
            a=i*pi/4
            b.beam('WheelSpoke',(w*.25,0,1.23),(w*.25+.37*cos(a),0,1.23+.37*sin(a)),.022,'Wood')
        b.box('TreasureChest',(w*.22,-d*.18,.65),(.65,.44,.45),'Wood',collision=True)
        _steps(b,'ShipEntry',0,d*.33,w*.24,.38)
        b.guide('WheelOptionalAnimation',(w*.25,0,1.23))
    elif n==55:
        for s in [-1,1]:
            b.box('SurfWallPost',(s*w*.45,-d*.23,h*.43),(.18,.18,h*.86),'Wood',collision=True)
        for zz in [.35,.65]:
            b.box('SurfWallRail',(0,-d*.23,h*zz),(w*.89,.13,.18),'Teal')
        colors=['Cream','Amber','Coral','Green','Blue']
        for i in range(5):
            x=(i-2)*w*.17; bh=h*(.76 if i%2 else .82); bw=w*.12
            profile=[(x+bw*.50*cos(a),.06+bh*.5+bh*.5*sin(a)) for a in [j*2*pi/32 for j in range(32)]]
            _prism(b,'Surfboard',profile,0,.12,colors[i],collision=True)
            b.box('SurfboardStringer',(x,.067,bh*.52),(bw*.065,.015,bh*.86),'Cream')
            for j in range(3):
                points=[(x-bw*.44+k*bw*.88/14,.076,bh*(.39+j*.065)+.028*sin(k*pi/7)) for k in range(15)]
                b.curve('BoardWaveDecal',points,.012,'Teal' if i==0 else 'Cream')
        _banner(b,'GoodWaves','GOOD WAVES',0,-d*.2,h*.94,w*.66,h*.12,'Wood')
        b.guide('SurfPhotoPosition',(0,d*.46,.05))
    elif n==56:
        _deck(b,'LookoutDeck',w*.96,d*.90,.30)
        for s in [-1,1]:
            b.box('OpenFrameUpright',(s*w*.39,-d*.23,h*.57),(.24,.25,h*.78),'Wood',collision=True)
        b.box('OpenFrameLintel',(0,-d*.23,h*.95),(w*.82,.25,.24),'Wood',collision=True)
        b.box('OpenFrameSill',(0,-d*.23,h*.18),(w*.82,.25,.18),'Wood',collision=True)
        _bench(b,'LookoutBench',-w*.12,d*.06,w*.47,z=.31,back=False)
        b.cyl('TelescopePedestal',(w*.31,d*.11,.91),.075,1.2,'Teal',collision=True)
        b.cyl('TelescopeBody',(w*.31,d*.09,1.57),.13,.48,'Cream',rotation=(pi/2,0,0))
        b.cyl('TelescopeLens',(w*.31,-d*.155,1.57),.105,.025,'Glass',rotation=(pi/2,0,0))
        _steps(b,'LookoutSteps',0,d*.45,w*.25,.30)
        b.guide('SunsetViewDirection_NegativeY',(0,-d*.6,h*.54))
        b.guide('TelescopeOptionalRotation',(w*.31,d*.09,1.50))
    elif n==57:
        b.box('PhotoBoothFloor',(0,0,.08),(w*.85,d*.85,.16),'Cream',collision=True)
        b.box('PhotoBoothBack',(0,-d*.36,h*.45),(w*.82,.15,h*.88),'Coral',collision=True)
        for s in [-1,1]:
            b.box('PhotoBoothSide',(s*w*.37,0,h*.45),(.15,d*.75,h*.88),'Cream',collision=True)
        for i in range(8):
            b.box('PinkStripedAwning',(-w*.45+(i+.5)*w*.9/8,d*.14,h*.83),(w*.9/8,d*.65,.19),'Pink' if i%2 else 'Cream')
        _curtains(b,'PhotoCurtain',w*.68,d*.32,.18,h*.75,'Coral')
        b.cyl('PhotoSeat',(0,0,.51),.25,.13,'DarkTeal',collision=True)
        b.cyl('PhotoSeatPedestal',(0,0,.28),.06,.43,'Metal')
        b.box('CameraSign',(0,-d*.08,h*.94),(w*.42,.22,h*.15),'DarkTeal')
        b.cyl('CameraLens',(0,d*.045,h*.94),h*.063,.08,'Glass',rotation=(pi/2,0,0))
        b.box('WaitingDisplay',(w*.34,d*.385,h*.55),(w*.16,.025,h*.22),'Screen')
        b.box('PrintSlot',(w*.34,d*.39,h*.29),(w*.12,.025,.08),'DarkTeal')
        _banner(b,'PhotoLabel','PHOTO',0,d*.48,h*.76,w*.65,.25,'DarkTeal')
        b.guide('CaptureCamera',(0,-d*.25,h*.56))
        b.guide('PhotoCurtainRuntime',(0,d*.31,h*.75))
        b.guide('PhotoSeatUse',(0,0,.62))
    elif n==58:
        for s in [-1,1]:
            b.box('ShadowWallPost',(s*w*.43,-d*.2,h*.48),(.18,.18,h*.96),'Wood',collision=True)
            _lantern(b,'ShadowSideLantern',(s*w*.40,d*.24,h*.43),.9)
        b.box('IndependentShadowCanvas',(0,-d*.21,h*.53),(w*.80,.05,h*.75),'Cream',collision=True)
        b.box('CanvasTopRail',(0,-d*.20,h*.93),(w*.89,.18,.12),'Wood')
        for s in [-1,1]:
            b.box('ShadowSeat',(s*w*.21,d*.20,.22),(w*.30,d*.25,.44),'Teal',collision=True)
            b.box('ShadowSeatTop',(s*w*.21,d*.20,.45),(w*.30,d*.25,.07),'Wood')
        b.guide('ShadowProjectorDirection',(0,d*.55,h*.52))
        b.guide('ShadowPlayerPosition',(0,0,0))


def _dome(b,name,x,y,z,rx,ry,h,mat, ribs=False):
    rings=8;segments=24
    verts=[]
    for j in range(rings):
        a=j*pi/(2*rings)
        for i in range(segments):
            t=i*2*pi/segments
            ripple=1+.045*cos(t*8) if ribs else 1
            verts.append((x+rx*cos(a)*cos(t)*ripple,y+ry*cos(a)*sin(t)*ripple,z+h*sin(a)))
    faces=[]
    for j in range(rings-1):
        for i in range(segments):
            a=j*segments+i;aa=j*segments+(i+1)%segments
            faces.append((a,aa,aa+segments,a+segments))
    apex=len(verts);verts.append((x,y,z+h))
    for i in range(segments):
        faces.append(((rings-1)*segments+i,(rings-1)*segments+(i+1)%segments,apex))
    faces.append(tuple(reversed(range(segments))))
    return b.mesh(name,verts,faces,mat)


def _lights(b,e,w,d,h):
    n=e['number']
    if n==59:
        b.cyl('JellyfishBase',(0,0,.10),min(w,d)*.43,.20,'Wood',vertices=36,collision=True)
        b.box('JellyfishMast',(0,-d*.06,h*.48),(.22,.22,h*.96),'Teal',collision=True)
        for i in range(3):
            a=i*2*pi/3+pi/2
            x=w*.31*cos(a);y=d*.31*sin(a);z=h*(.75 if i%2 else .69)
            b.beam('JellyfishArm',(0,-d*.06,h*.85),(x,y,h*.85),.05,'Teal')
            b.beam('JellyfishPendant',(x,y,h*.85),(x,y,z+.19),.018,'Metal')
            _dome(b,'JellyfishBell',x,y,z,w*.15,d*.15,h*.12,'Cream',True)
            _ring(b,'JellyfishBellRim',x,y,z,w*.15,w*.135,.035,'Teal')
            for j in range(7):
                aa=j*2*pi/7;length=h*(.25+.025*(j%3))
                points=[(x+w*.10*cos(aa)+.035*sin(t*pi*4/14+j),y+d*.1*sin(aa)+.025*cos(t*pi*3/14),z-length*t/14) for t in range(15)]
                b.curve('JellyfishTentacle',points,.024,'Amber')
            b.guide('Light_Jellyfish_'+str(i),(x,y,z+.04))
    elif n==60:
        b.box('WaveLightPlinth',(0,0,.10),(w*.96,d*.66,.20),'Teal',bevel=.10,collision=True)
        for i in range(3):
            hh=h*(.93-.19*i);x0=-w*.30+i*w*.28
            # A curling strip forms a readable wave crest in the daytime.
            outline=[(x0-w*.16,.2),(x0-w*.12,hh*.35),(x0-w*.10,hh*.66),(x0-w*.04,hh*.91),
                     (x0+w*.08,hh),(x0+w*.19,hh*.91),(x0+w*.17,hh*.79),(x0+w*.09,hh*.87),
                     (x0+w*.015,hh*.74),(x0+w*.02,hh*.45),(x0+w*.13,.2)]
            _prism(b,'WaveCrest',outline,(i-1)*d*.10,d*.18,'Blue' if i%2 else 'Teal')
            b.box('WaveSupport',(x0-w*.03,(i-1)*d*.10,hh*.26),(w*.11,d*.14,hh*.49),'Teal',collision=True)
            b.curve('IndependentWaveGlow',[(x,y+d*.095,z) for x,z in outline[:7] for y in [(i-1)*d*.10]],.035,'Amber')
            b.sphere('CrestGlobe',(x0+w*.075,(i-1)*d*.10,hh+.08),(.12,.12,.12),'Amber')
            b.guide('Light_Wave_'+str(i),(x0+w*.075,(i-1)*d*.10,hh+.08))
    elif n==61:
        for s in [-1,1]:
            _plant(b,'LanternLinePlanter',s*w*.43,0,size=min(d,w*.20))
            b.box('LanternLinePost',(s*w*.43,0,h*.50),(.15,.15,h*.95),'Teal',collision=True)
            b.beam('LanternHookArm',(s*w*.43,0,h*.92),(s*w*.33,0,h*.92),.05,'Teal')
        wire=[(-w*.37+i*w*.74/32,0,h*.90-h*.18*sin(pi*i/32)) for i in range(33)]
        b.curve('SaggingLanternWire',wire,.012,'DarkTeal')
        for i in range(7):
            x=-w*.35+i*w*.70/6;zz=h*.90-h*.18*sin(pi*(i+.15)/6.3)
            b.beam('LanternDrop',(x,0,zz),(x,0,zz-.12),.010,'Metal')
            _lantern(b,'WishLantern_'+str(i),(x,0,zz-.33),.78,'Teal' if i%2 else 'Cream',physical=i in [1,3,5])
    elif n==62:
        for sx in [-1,1]:
            for sy in [-1,1]:
                b.cyl('ShellPendantMast',(sx*w*.39,sy*d*.37,h*.49),.09,h*.98,'Wood',collision=True)
        for sy in [-1,1]:
            pts=[(-w*.39+i*w*.78/20,sy*d*.37,h*.93-.18*sin(pi*i/20)) for i in range(21)]
            b.curve('PendantSupportRope',pts,.025,'Wood')
        for i,(x,y,scale) in enumerate([(-w*.24,-d*.22,.8),(w*.21,-d*.20,.65),(0,d*.1,1.05),(-w*.27,d*.22,.60),(w*.28,d*.23,.7)]):
            zz=h*(.58+(i%2)*.14)
            edge_z=h*.93-.18*sin(pi*(x/(w*.78)+.5))
            cross_rope=[(x,-d*.37,edge_z),(x,0,edge_z-.08),(x,d*.37,edge_z)]
            b.curve('ShellOverheadSupportRope',cross_rope,.018,'Wood')
            hanger_top=edge_z-.08*(1-abs(y)/(d*.37))
            b.beam('ShellDrop',(x,y,hanger_top),(x,y,zz+.27*scale),.015,'Wood')
            _dome(b,'ScallopedShellShade',x,y,zz,.45*scale,.42*scale,.35*scale,'Sand' if i%2==0 else 'Pink',True)
            b.sphere('ShellInnerGlow',(x,y,zz+.02),(.27*scale,.27*scale,.09*scale),'Amber')
            for j in range(9):
                a=j*2*pi/9
                b.curve('ShellRib',[(x+.43*scale*cos(a)*cos(t*pi/16),y+.40*scale*sin(a)*cos(t*pi/16),zz+.34*scale*sin(t*pi/16)) for t in range(9)],.015,'Cream')
            if i<3:
                b.guide('Light_ShellPendant_'+str(i),(x,y,zz))
        for s in [-1,1]:
            _plant(b,'ShellGarden',s*w*.39,s*d*.37,size=.45)
    elif n==63:
        b.cyl('LighthouseLampFoot',(0,0,.15),min(w,d)*.26,.30,'Teal',vertices=8,collision=True)
        for i in range(8):
            b.cyl('StripedLampColumn',(0,0,.30+(i+.5)*(h*.62)/8),min(w,d)*.10,h*.62/8,
                  'Teal' if i%3==0 else 'Cream',collision=i<7)
        for s in [-1,1]:
            b.beam('LampArm',(0,0,h*.78),(s*w*.37,0,h*.78),.035,'Teal')
            b.beam('LampArmBrace',(0,0,h*.67),(s*w*.32,0,h*.78),.025,'Teal')
            side_scale=min(1.1,h*.22)
            b.beam('SideLanternHanger',(s*w*.37,0,h*.78),(s*w*.37,0,h*.68+.4*side_scale),.018,'Teal')
            _lantern(b,'SideLighthouseLantern',(s*w*.37,0,h*.68),side_scale)
        crown_scale=min(1.0,h*.20)
        lower=.30+h*.62;upper=h*.88-.18*crown_scale
        b.cyl('CrownMastConnector',(0,0,(lower+upper)/2),min(w,d)*.075,upper-lower+.035,'Teal',collision=True)
        _lantern(b,'CrownLighthouseLantern',(0,0,h*.88),crown_scale)
    elif n==64:
        r=min(w,d)*.24
        b.box('BollardFoot',(0,0,h*.10),(r*2.3,r*2.3,h*.20),'Teal',collision=True)
        b.cyl('BollardTimber',(0,0,h*.46),r,h*.60,'Wood',vertices=12,collision=True)
        for zz in [.37,.40,.43]:
            b.torus('BollardRope',(0,0,h*zz),r*1.04,.024,'Cream',rotation=(0,0,0))
        b.box('BollardLanternBase',(0,0,h*.73),(r*2.5,r*2.5,.06),'Teal')
        b.box('BollardGlow',(0,0,h*.84),(r*1.9,r*1.9,h*.20),'Amber')
        for sx in [-1,1]:
            for sy in [-1,1]:
                b.beam('BollardCage',(sx*r,sy*r,h*.73),(sx*r,sy*r,h*.95),.02,'Teal')
        _cone(b,'BollardCap',0,0,h*.95,r*1.7,h*.06,'Teal',4)
        b.guide('Light_Bollard',(0,0,h*.84))


def _floor_patch(b,name,vertices,mat,z):
    # Planar patches are baked into the joined surface, not protruding props.
    area=sum(a[0]*c[1]-c[0]*a[1] for a,c in zip(vertices,vertices[1:]+vertices[:1]))
    if area<0:
        vertices=list(reversed(vertices))
    return b.mesh(name,[(x,y,z) for x,y in vertices],[tuple(range(len(vertices)))],mat)


def _floors(b,e,w,d,h):
    n=e['number'];z=max(h,.02)
    base='DarkTeal' if n==67 else 'Sand' if n in [66,68,70] else 'Wood'
    b.box('FlushFloorFoundation',(0,0,(z-.0011)*.5),(w,d,z-.0011),base,bevel=0,collision=True)
    top=z-.0003
    if n==65:
        for i in range(8):
            x0=-w*.5+i*w/8;x1=x0+w/8
            _floor_patch(b,'StripePattern',[(x0,-d*.46),(x1,-d*.46),(x1,d*.46),(x0,d*.46)],'Blue' if i%2 else 'Cream',top)
        for s in [-1,1]:
            _floor_patch(b,'FlushStripeBorder',[(-w/2,s*d*.5),(-w/2,s*d*.46),(w/2,s*d*.46),(w/2,s*d*.5)],'Cream',top)
    elif n==66:
        for i in range(3):
            x=-w/3+i*w/3;r=min(w/7,d*.35)
            for j in range(7):
                a=pi*.10+j*pi*.8/7;aa=pi*.10+(j+1)*pi*.8/7
                _floor_patch(b,'ScallopShellInlay',[(x,-r*.48),(x+r*cos(a),-r*.48+r*sin(a)),
                                                  (x+r*cos(aa),-r*.48+r*sin(aa))],
                             'Teal' if j%2 else 'Cream',top+.0003)
            for j in range(12):
                xx=-w/2+j*w/11;yy=d*.37+.03*sin(j*pi/2)
                if i==0:
                    _floor_patch(b,'WaveBorderInlay',[(xx,yy),(xx+w/22,yy+.02),(xx+w/22,yy+.055),(xx,yy+.035)],'Teal',top+.0003)
    elif n==67:
        for s in [-1,1]:
            _floor_patch(b,'IndependentEmissiveBoundary',[(-w/2,s*d*.42),(w/2,s*d*.42),(w/2,s*d*.46),(-w/2,s*d*.46)],'Amber',top+.0002)
        for i in range(7):
            for j in range(4):
                xa=-w/2+i*w/7;ya=-d*.38+j*d*.76/4
                _floor_patch(b,'NightPavingPanel',[(xa+.01,ya+.01),(xa+w/7-.01,ya+.01),
                                                 (xa+w/7-.01,ya+d*.19-.01),(xa+.01,ya+d*.19-.01)],'DarkTeal' if (i+j)%2 else 'Blue',top)
        b.guide('BoundaryLightRuntime',(0,0,z))
    elif n==68:
        # Four broad spiral ribbons, each a single mesh rather than mosaic debris.
        for band in range(4):
            verts=[];faces=[]
            for j in range(65):
                t=j/64;ang=t*pi*2.15+band*pi/2
                r=.025+t*.48
                for dr in [-.035,.035]:
                    rr=max(0,r+dr)
                    verts.append((cos(ang)*rr*w,sin(ang)*rr*d,top+.0003))
            for j in range(64):
                faces.append((j*2,j*2+1,j*2+3,j*2+2))
            b.mesh('BroadSpiralWaveInlay',verts,faces,'Teal' if band%2 else 'Blue')
    elif n==69:
        for i in range(12):
            yy=-d*.46+i*d*.92/12
            _floor_patch(b,'DeckPlankSurface',[(-w*.48,yy+.008),(w*.48,yy+.008),(w*.48,yy+d*.92/12-.008),(-w*.48,yy+d*.92/12-.008)],
                         'Wood' if i%3 else 'DarkWood',top)
        for s in [-1,1]:
            _floor_patch(b,'FlushDeckMetalEdge',[(-w/2,s*d*.5),(w/2,s*d*.5),(w/2,s*d*.46),(-w/2,s*d*.46)],'Metal',top)
            _floor_patch(b,'FlushDeckMetalEnd',[(s*w*.5,-d*.46),(s*w*.48,-d*.46),(s*w*.48,d*.46),(s*w*.5,d*.46)],'Teal',top)
    elif n==70:
        for ix in range(2):
            for iy in range(2):
                x0=-w/2+ix*w/2;y0=-d/2+iy*d/2
                if ix==iy:
                    _floor_patch(b,'FlushGrassBlock',[(x0,y0),(x0+w/2,y0),(x0+w/2,y0+d/2),(x0,y0+d/2)],'Green',top)
                else:
                    for i in range(3):
                        for j in range(3):
                            xa=x0+i*w/6;ya=y0+j*d/6
                            _floor_patch(b,'FlushPicnicPaver',[(xa+.008,ya+.008),(xa+w/6-.008,ya+.008),
                                                             (xa+w/6-.008,ya+d/6-.008),(xa+.008,ya+d/6-.008)],'Cream',top)
    b.guide('FloorTravel_PositiveX',(w*.5,0,z))


def build(b, entry):
    """Build exactly one requested environment facility in the caller's scope."""
    n=int(entry['number'])
    footprint=entry.get('footprint_cm',[400,400])
    if isinstance(footprint,dict):
        w=float(footprint.get('x',footprint.get('width',400)))/100
        d=float(footprint.get('y',footprint.get('depth',400)))/100
    else:
        w,d=[float(v)/100 for v in footprint[:2]]
    h=float(entry.get('height_cm',300))/100
    if 32<=n<=39:
        _rest(b,entry,w,d,h)
    elif 40<=n<=46:
        _performance(b,entry,w,d,h)
    elif 47<=n<=50:
        _safety(b,entry,w,d,h)
    elif 51<=n<=58:
        _photo(b,entry,w,d,h)
    elif 59<=n<=64:
        _lights(b,entry,w,d,h)
    elif 65<=n<=70:
        _floors(b,entry,w,d,h)
    else:
        raise ValueError('Environment builder handles facilities 32--70, got %s' % n)
