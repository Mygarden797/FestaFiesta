# expansion70_geometry_services.py
# 역할: 체험·서비스 시설의 편집 가능한 형상을 만든다.

"""Editable, reference-led facility parts for brief entries 8 through 31.

Geometry is in metres, customer front is +Y, and the floor sits at Z=0.
These static studies leave interaction, animation and service logic to Unreal.
"""
import math


def sign(b, name, label, x, y, z, width, accent='Teal', height=.52):
    b.box(name+'Board',(x,y,z),(width,.16,height),accent)
    b.box(name+'Inset',(x,y+.09,z),(width-.10,.02,height-.10),'DarkTeal')
    b.text(name+'Label',label,(x,y+.115,z),min(height*.35,width/(max(map(len,label.split('\n')))*.75)),'Cream')


def stool(b, name, x, y, color='Teal'):
    b.cyl(name+'Seat',(x,y,.52),.25,.1,color)
    for dx in (-.15,.15):
        for dy in (-.15,.15):
            b.box(name+'Leg',(x+dx,y+dy,.26),(.065,.065,.48),'Wood')
    b.guide(name+'SeatedPosition',(x,y,.62))


def plant(b,name,x,y,z=0):
    b.box(name+'Planter',(x,y,z+.24),(.50,.50,.48),'Cream',collision=True)
    b.box(name+'Soil',(x,y,z+.49),(.43,.43,.03),'DarkWood')
    for i in range(5):
        a=i*math.tau/5
        b.sphere(name+'Leaf',(x+.16*math.cos(a),y+.16*math.sin(a),z+.76),(.14,.23,.35),'Green')


def canopy(b,name,width,depth,height=3.4,accent='Teal',back=False):
    b.box(name+'Deck',(0,0,.075),(width,depth,.15),'Wood',collision=True)
    for x in (-width*.46,width*.46):
        for y in (-depth*.44,depth*.44):
            b.box(name+'Post',(x,y,height*.45),(.15,.15,height*.90),accent,collision=True)
    corners=[(-width/2,-depth/2,height*.88),(width/2,-depth/2,height*.88),
             (width/2,depth/2,height*.88),(-width/2,depth/2,height*.88)]
    for i in range(4):
        a,c=corners[i],corners[(i+1)%4]
        b.mesh(name+'CanopyPanel',[a,c,(0,0,height),(a[0],a[1],a[2]-.05),
               (c[0],c[1],c[2]-.05),(0,0,height-.05)],
               [(0,1,2),(5,4,3),(0,3,4,1),(1,4,5,2),(2,5,3,0)],accent if i%2==0 else 'Cream')
        b.beam(name+'Edge',a,c,.055,'Metal')
    if back:
        b.box(name+'Back',(0,-depth*.46,1.45),(width*.90,.1,2.60),'Cream',collision=True)


def shelves(b,name,x,y,z,width=2,rows=3):
    for r in range(rows):
        b.box(name+'Shelf',(x,y,z+r*.40),(width,.42,.065),'Wood')
        for c in range(5):
            color=['Cream','Coral','Blue','Teal','Sand'][(c+r)%5]
            b.box(name+'Display',(x+(c-2)*width/5,y+.02,z+.14+r*.40),(.22,.12,.22),color)


def shorten_kiosk_awning(b, name, centre_y, depth):
    """Keep rear rain cover while exposing the facility's activity display."""
    for obj in b.vis.objects:
        if obj.get('FF70.Part') == name+'AwningStripe':
            original_depth = obj.dimensions.y
            obj.scale.y *= depth / original_depth
            obj.location.y = centre_y
        elif obj.get('FF70.Part') == name+'Header':
            # The original blank centre header must not become an opaque strip
            # across the now-open activity display after shortening the roof.
            obj.location.y = centre_y-depth*.5-.09
            obj.location.z = 2.54


def curved_bar(b):
    # Closed convex body wedges meet without the dark gaps from rotated boxes.
    outer_x,outer_y,inner_x,inner_y=2.10,1.20,1.42,.57
    def point(a,outer,z,overhang=0):
        rx,ry=(outer_x+overhang,outer_y+overhang) if outer else (inner_x-overhang,inner_y-overhang)
        return (rx*math.cos(a),ry*math.sin(a)-.28,z)
    for i in range(12):
        a,c=i*math.pi/12,(i+1)*math.pi/12
        quad=[point(a,True,.14),point(c,True,.14),point(c,False,.14),point(a,False,.14)]
        verts=quad+[(x,y,1.33) for x,y,z in quad]
        b.mesh('CurvedBarBody',verts,[(0,3,2,1),(4,5,6,7),(0,1,5,4),
               (1,2,6,5),(2,3,7,6),(3,0,4,7)],'Teal',collision=True)
    # One continuous top, explicitly wound outward, with a small overhang.
    verts=[]
    segments=36
    for z in (1.33,1.45):
        for outer in (True,False):
            verts.extend(point(i*math.pi/segments,outer,z,.055) for i in range(segments+1))
    stride=segments+1; faces=[]
    for i in range(segments):
        faces.extend([(i,i+stride,i+1+stride,i+1),
                      (i+2*stride,i+1+2*stride,i+1+3*stride,i+3*stride),
                      (i,i+1,i+1+2*stride,i+2*stride),
                      (i+stride,i+3*stride,i+1+3*stride,i+1+stride)])
    faces.extend([(0,2*stride,3*stride,stride),
                  (segments,segments+stride,segments+3*stride,segments+2*stride)])
    b.mesh('ContinuousCurvedBarTop',verts,faces,'Wood')


def shell(b,name,x,y,z,radius=.35,color='Cream'):
    # Broad fan-shaped ribs distinguish the shell without tiny geometry.
    for i in range(7):
        a=-math.pi*.43+i*math.pi*.86/6
        b.beam(name+'Rib',(x,y,z),(x+math.sin(a)*radius,y,z+math.cos(a)*radius),radius*.16,color)


def workshop(b,e,kind):
    w,d=[n/100 for n in e['footprint_cm']]
    canopy(b,'Workshop',w*.92,d*.94,e['height_cm']/100*.92,'Coral' if kind=='shell' else 'Teal',True)
    b.table('Worktable',0,.20,w*.70,1.0)
    b.box('RearWorkbench',(0,-d*.35,1.0),(w*.78,.65,.14),'Wood',collision=True)
    shelves(b,'Samples',0,-d*.40,1.50,w*.70,2)
    for i in range(4):
        x=(i-1.5)*w*.17
        stool(b,'GuestStool',x,1.12)
        b.box('WorkMat',(x,.18,.855),(.55,.48,.025),'Blue' if kind=='resin' else 'Cream')
        b.cyl('ToolCup',(x-.18,-.10,.98),.07,.18,'Teal')
        for j in range(3):b.beam('Brush',(x-.18+j*.024,-.10,1.03),(x-.21+j*.025,-.10,1.24),.01,'Wood')
        if kind=='shell':
            b.torus('Bracelet',(x,.16,.90),.17,.035,'Cream',(0,0,0))
            shell(b,'TableShell',x+.11,.15,.91,.09)
        elif kind=='resin':
            b.box('Mould',(x,.18,.885),(.23,.28,.025),'Pink')
            b.box('ResinPiece',(x,.18,.91),(.15,.19,.02),'Blue')
        else:
            shell(b,'PaintedFan',x,.15,.90,.22,'Coral')
    label={'shell':'SHELL BRACELETS','resin':'WAVE RESIN','fan':'MY SUMMER FAN'}[kind]
    front=d*.47+.18
    sign(b,'WorkshopHeader',label,0,front,2.91,w*.68)
    if kind=='shell':shell(b,'RoofShell',0,front+.12,3.20,.58)
    elif kind=='resin':
        b.torus('Keyring',(0,front+.12,3.63),.20,.04,'Metal')
        b.sphere('KeyringWave',(0,front+.12,3.26),(.39,.11,.27),'Blue')
        b.curve('KeyringFoam',[(-.31,front+.26,3.25),(-.13,front+.26,3.38),(.05,front+.26,3.21),(.29,front+.26,3.32)],.032,'Cream')
    else:shell(b,'RoofFan',0,front+.12,3.14,.80,'Cream')


def simple_door(b,name,x,y,width=1,height=2.15,color='Teal'):
    b.box(name+'Door',(x,y,height/2+.14),(width,.09,height),color,collision=True)
    b.box(name+'TopFrame',(x,y+.03,height+.2),(width+.20,.12,.12),'Cream')
    for dx in (-width/2-.055,width/2+.055):b.box(name+'Jamb',(x+dx,y,height/2+.13),(.11,.16,height+.12),'Cream')
    b.box(name+'Handle',(x+width*.34,y+.10,1.10),(.055,.065,.24),'Metal')
    b.box(name+'Status',(x+width*.30,y+.08,1.47),(.13,.03,.08),'Green')
    b.guide(name+'Hinge',(x-width/2,y,.15))
    b.guide(name+'Entry',(x,y+.85,0))


def person_icon(b,name,x,y,z,scale=.6,color='Cream',child=False):
    b.sphere(name+'Head',(x,y,z+scale*.35),(scale*.12,.025,scale*.12),color)
    b.box(name+'Torso',(x,y,z),(scale*.25,.04,scale*.38),color)
    for dx in (-scale*.075,scale*.075):b.box(name+'Leg',(x+dx,y,z-scale*.30),(scale*.075,.04,scale*.29),color)
    for dx in (-scale*.19,scale*.19):b.box(name+'Arm',(x+dx,y,z+.01),(scale*.08,.04,scale*.29),color)


def wheelchair(b,name,x,y,z,scale=.65):
    b.torus(name+'Wheel',(x-.06,y,z-scale*.18),scale*.28,scale*.043,'Cream')
    b.sphere(name+'Head',(x+.07,y,z+scale*.43),(scale*.10,.025,scale*.10),'Cream')
    b.beam(name+'Back',(x,y,z+scale*.30),(x,y,z-.05),scale*.047,'Cream')
    b.beam(name+'Seat',(x,y,z-.05),(x+scale*.28,y,z-.05),scale*.047,'Cream')
    b.beam(name+'Leg',(x+scale*.28,y,z-.05),(x+scale*.39,y,z-scale*.30),scale*.047,'Cream')


def toilet_fixture(b,name,x,y,scale=1):
    b.box(name+'Tank',(x,y-.18,.65*scale),(.40*scale,.24*scale,.62*scale),'White')
    b.sphere(name+'Bowl',(x,y+.11,.40*scale),(.25*scale,.34*scale,.21*scale),'White')
    b.torus(name+'Seat',(x,y+.11,.55*scale),.20*scale,.045*scale,'Cream',(0,0,0))
    b.cyl(name+'Pedestal',(x,y+.04,.25*scale),.15*scale,.45*scale,'White')


def sink(b,name,x,y,z=.90):
    b.box(name+'Basin',(x,y,z),(.60,.42,.13),'White')
    b.box(name+'Inset',(x,y+.01,z+.068),(.40,.25,.015),'Glass')
    b.curve(name+'Tap',[(x,y-.10,z),(x,y-.10,z+.26),(x,y+.04,z+.26),(x,y+.04,z+.18)],.025,'Metal')
    b.box(name+'Support',(x,y-.15,z*.55),(.18,.15,z*.80),'Cream')


def build(b, entry):
    n=entry['number']
    w,d=[v/100 for v in entry['footprint_cm']]
    h=entry['height_cm']/100
    if n==8:
        b.truck('SouvenirTruck')
        shelves(b,'Postcards',-.55,-.45,1.45,2.95,3)
        sign(b,'Header','LOCAL GOODS',-.3,.20,3.20,3.35)
        b.box('MagnetBoard',(-1.85,.78,1.98),(.45,.05,.96),'Wood')
        for i in range(6):b.cyl('SouvenirMagnet',(-1.95+(i%2)*.20,.825,1.7+(i//2)*.27),.055,.025,['Blue','Coral','Teal'][i%3],12,(math.pi/2,0,0))
        for x in (-1.2,-.3,.6):b.box('GiftBox',(x,1.2,1.50),(.34,.27,.26),'Cream')
        b.beam('FlagPole',(.95,0,2.90),(.95,0,3.72),.028,'Metal')
        b.mesh('LocalPennant',[(.95,0,3.70),(1.65,0,3.46),(.95,0,3.32)],[(0,1,2)],'Teal')
    elif n==9:
        b.kiosk('Clothing',4.2,3.0,'Coral')
        shorten_kiosk_awning(b,'Clothing',-.67,1.58)
        b.beam('FrontClothingFrame',(-1.89,1.05,2.66),(1.89,1.05,2.66),.055,'Metal')
        b.beam('GarmentRail',(-1.7,.20,2.40),(1.7,.20,2.40),.025,'Metal')
        sign(b,'Fashion','FESTIVAL FASHION',0,-.60,3.02,3.40,'Coral')
        for i,x in enumerate((-1.18,0,1.18)):
            z=2.0
            verts=[(x-.36,.23,z+.32),(x-.57,.23,z+.16),(x-.42,.23,z-.07),(x-.27,.23,z+.02),
                   (x-.27,.23,z-.47),(x+.27,.23,z-.47),(x+.27,.23,z+.02),(x+.42,.23,z-.07),
                   (x+.57,.23,z+.16),(x+.36,.23,z+.32),(x+.12,.23,z+.26),(x-.12,.23,z+.26)]
            b.mesh('DisplayTShirt',verts,[tuple(range(12))],['Teal','Cream','Blue'][i])
            b.text('ShirtLogo','FF',(x,.27,z-.08),.16,'Coral')
            b.beam('Hanger',(x-.26,.20,z+.35),(x+.26,.20,z+.35),.015,'Metal')
        for x in (-1.40,1.40):
            for z in (1.35,1.60,1.85):
                b.sphere('CapCrown',(x,.38,z),(.25,.22,.16),'Cream')
                b.box('CapBrim',(x,.58,z-.05),(.36,.28,.04),'Teal')
        for i in range(4):b.box('FoldedShirt',(-.25,.83,1.30+i*.055),(.6,.36,.05),['Cream','Blue','Coral','Teal'][i])
    elif n==10:
        b.truck('GlowGoodsTruck')
        sign(b,'GlowHeader','GLOW TOGETHER',-.4,.20,3.23,3.50)
        for i in range(8):
            x=-1.75+i*.4
            b.cyl('DisplayGlowStick',(x,-.35,1.96),.045,.72,['Teal','Pink','Blue','Amber'][i%4])
            b.cyl('GlowGrip',(x,-.35,1.58),.07,.20,'Metal')
        for i in range(6):b.torus('BraceletDisplay',(-1.2+i*.40,.72,1.54),.12,.025,['Teal','Pink','Amber'][i%3])
        for x,a in ((-1.75,-.25),(1.10,.25)):
            b.cyl('RoofGlowStick',(x,.10,3.36),.11,1.10,'Amber',24,(0,a,0))
            b.cyl('RoofGlowGrip',(x,.10,2.92),.14,.27,'DarkTeal')
        b.guide('GlowDisplayLight',(-.4,.9,2.30))
    elif n in (11,12,13):
        workshop(b,entry,{11:'shell',12:'resin',13:'fan'}[n])
    elif n==14:
        b.kiosk('Dart',4.2,3.30)
        shorten_kiosk_awning(b,'Dart',-1.17,.90)
        sign(b,'DartHeader','BALLOON DART',0,-.71,3.30,3.2)
        for x in (-1.40,1.40):
            b.beam('DartSignStrut',(x,-1.19,2.53),(x,-.75,3.29),.035,'Metal')
        b.box('BalloonTargetBoard',(-.60,-1.30,1.76),(2.65,.12,1.65),'DarkTeal')
        for row in range(3):
            for col in range(5):
                x=-1.65+col*.53; z=1.20+row*.48
                b.sphere('Balloon_%d_%d'%(row,col),(x,-1.14,z),(.18,.11,.22),['Coral','Blue','Amber','Green','Pink'][(row+col)%5])
                b.guide('BalloonTarget_%d_%d'%(row,col),(x,-.99,z))
        shelves(b,'PrizeShelves',1.42,-1.1,1.10,.68,3)
        for z in (1.48,1.90,2.32):
            b.sphere('TeddyPrize',(1.4,-.92,z),(.18,.13,.19),'Cream')
            for dx in (-.12,.12):b.sphere('TeddyEar',(1.4+dx,-.92,z+.17),(.065,.05,.075),'Cream')
        b.box('ThrowLine',(0,1.76,.025),(3.35,.07,.025),'Cream')
        b.guide('ThrowPosition',(0,1.74,0))
    elif n==15:
        b.box('RangeDeck',(0,0,.07),(w*.95,d*.95,.14),'Wood',collision=True)
        for x in (-w*.46,w*.46):
            for y in (-d*.43,0,d*.43):b.box('SafetyPost',(x,y,1.65),(.16,.16,3.15),'Teal',collision=True)
            for z in (.75,1.30,1.85,2.40,2.95):b.beam('NetHorizontal',(x,-d*.43,z),(x,d*.43,z),.012,'Metal')
            for j in range(17):b.beam('NetVertical',(x,-d*.43+j*d*.86/16,.3),(x,-d*.43+j*d*.86/16,3.05),.012,'Metal')
        b.box('Backstop',(0,-d*.44,1.55),(w*.90,.16,2.8),'Cream',collision=True)
        for i,x in enumerate((-2.30,0,2.30)):
            for radius,color in ((.72,'Blue'),(.53,'Cream'),(.33,'Coral'),(.14,'Amber')):
                b.cyl('TargetRing',(x,-d*.44+.12+(1-radius)*.045,1.86),radius,.03,color,32,(math.pi/2,0,0))
            b.box('ShootingLine',(x,d*.30,.16),(1.35,.08,.02),'Cream')
            b.box('BowStand',(x,d*.38,.78),(.5,.32,1.20),'Teal',collision=True)
            b.curve('DisplayBow',[(x-.22,d*.38,1.65),(x-.36,d*.38,1.25),(x-.22,d*.38,.92)],.025,'Wood')
            b.beam('BowString',(x-.22,d*.38,1.65),(x-.22,d*.38,.92),.005,'Cream')
            b.guide('Archer_%d'%i,(x,d*.30,0))
            b.guide('Target_%d'%i,(x,-d*.44+.3,1.86))
        for i in range(10):b.box('RearAwning',(-w*.45+i*w*.09,-d*.425,3.30),(w*.095,.95,.14),'Teal' if i%2==0 else 'Cream')
        sign(b,'Archery','MINI ARCHERY',0,-d*.36,3.58,w*.59)
    elif n==16:
        b.box('GameBase',(0,0,.20),(4.65,5.9,.40),'Coral',collision=True)
        b.box('BackPanel',(0,-2.65,2.04),(4.40,.15,3.56),'Blue',collision=True)
        for x in (-1.07,1.07):
            b.box('Backboard',(x,-2.48,2.72),(1.62,.12,1.18),'Cream')
            b.box('BackboardTarget',(x,-2.40,2.54),(.56,.025,.48),'Coral')
            b.box('BackboardTargetInner',(x,-2.38,2.55),(.43,.025,.35),'Cream')
            b.torus('Hoop',(x,-1.94,2.26),.35,.042,'Coral',(0,0,0))
            for j in range(10):
                a=j*math.tau/10
                b.beam('BasketNet',(x+.34*math.cos(a),-1.94+.34*math.sin(a),2.25),(x+.22*math.cos(a+.22),-1.94+.22*math.sin(a+.22),1.73),.014,'Cream')
            b.torus('NetLower',(x,-1.94,1.73),.22,.018,'Cream',(0,0,0))
            b.guide('HoopScoringCenter',(x,-1.94,2.24))
        b.mesh('BallReturnRamp',[(-2.12,-2.36,.80),(2.12,-2.36,.80),(2.12,2.23,.38),(-2.12,2.23,.38),
               (-2.12,-2.36,.63),(2.12,-2.36,.63),(2.12,2.23,.21),(-2.12,2.23,.21)],
               [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],'Wood',collision=True)
        for x in (-2.18,2.18):
            b.beam('NetFrame',(x,-2.53,3.70),(x,2.45,1.15),.045,'Metal')
            b.box('SideRail',(x,0,.87),(.16,5.2,.65),'Coral',collision=True)
            for j in range(9):
                y=-2.4+j*.57
                b.beam('SideNet',(x,y,1.08),(x,y,3.6-(y+2.4)*.50),.012,'Metal')
        b.box('BallTrough',(0,2.50,.77),(4.5,.4,.62),'Coral',collision=True)
        for x in (-1.35,-.55,.35,1.2):
            b.sphere('DisplayBasketball',(x,2.15,.83),(.25,.25,.25),'Coral')
            b.torus('BallSeam',(x,2.15,.83),.251,.006,'Metal')
        sign(b,'Basketball','BASKETBALL\nSHOT CHALLENGE',0,-2.35,4.10,4.05,'Coral',.80)
        b.guide('BallSpawn',(0,2.03,1.1))
    elif n==17:
        b.box('RhythmDeck',(0,0,.075),(4.8,3.2,.15),'DarkWood',collision=True)
        b.box('RhythmBack',(0,-1.38,1.46),(4.5,.14,2.75),'Cream',collision=True)
        for x in (-2.15,2.15):
            b.box('ArcadePost',(x,-1.18,1.55),(.13,.13,2.95),'Pink',collision=True)
        for i in range(10):
            b.box('ArcadeRearAwning',(-2.16+i*.48,-1.06,3.0),(.49,1.18,.12),'Pink' if i%2==0 else 'Cream')
        b.box('DisplayPedestal',(0,.22,.69),(3.5,.69,1.12),'Pink',collision=True)
        b.box('DisplayHousing',(0,.22,1.85),(3.5,.4,1.60),'DarkTeal',collision=True)
        b.box('VideoScreen',(0,.45,1.89),(2.85,.035,1.24),'Screen')
        b.text('ScreenStandIn','RHYTHM\nPERFECT!',(0,.48,1.89),.31,'Amber')
        for x in (-1.85,1.85):
            b.box('Speaker',(x,.36,1.95),(.42,.5,1.55),'Pink')
            for z in (1.52,2.19):b.cyl('SpeakerCone',(x,.625,z),.17,.03,'Metal',24,(math.pi/2,0,0))
        for i,x in enumerate((-1.14,1.14)):
            b.box('DancePad',(x,1.60,.14),(1.92,1.50,.28),'Metal',collision=True)
            for j,(dx,dy) in enumerate(((-.55,0),(.55,0),(0,-.43),(0,.43))):
                b.box('ArrowPanel',(x+dx,1.60+dy,.291),(.44,.35,.025),'Pink' if j%2 else 'Blue')
                b.mesh('Arrow',[(x+dx-.14,1.60+dy-.09,.31),(x+dx+.14,1.60+dy-.09,.31),(x+dx,1.60+dy+.13,.31)],[(0,1,2)],'Cream')
            b.curve('SupportHandrail',[(x-.62,2.17,.25),(x-.62,2.17,1.26),(x+.62,2.17,1.26),(x+.62,2.17,.25)],.045,'Coral')
            b.guide('RhythmPlayer_%d'%i,(x,1.60,.30))
        sign(b,'RhythmHeader','RHYTHM GAME',0,.42,3.06,3.65,'Pink')
        b.guide('ScreenMedia',(0,.50,1.89))
    elif n==18:
        canopy(b,'BoardTent',5.65,5.10,4.30,'Coral')
        b.table('BoardTable',0,0,2.30,1.50)
        b.box('GameBoard',(0,0,.855),(.90,.90,.025),'Teal')
        for i in range(6):
            for j in range(6):b.box('BoardSquare',(-.375+i*.15,-.375+j*.15,.873),(.145,.145,.013),'Cream' if (i+j)%2 else 'DarkTeal',bevel=0)
        for i,(x,y) in enumerate(((-.37,-.25),(.24,.34),(.37,-.14))):b.cyl('GamePiece',(x,y,.94),.045,.13,['Coral','Amber','Blue'][i])
        for x in (-1.12,0,1.12):
            stool(b,'FrontSeat',x,1.12,'Coral')
            stool(b,'RearSeat',x,-1.15)
        shelves(b,'GameBoxes',0,-2.1,.65,3.8,4)
        sign(b,'BoardGames','BOARD GAMES',0,2.69,3.54,3.3,'Coral')
        for i,color in enumerate(('Teal','Cream','Blue')):
            b.box('OversizedGameBox',((i-1)*.46,2.60,3.99),(.40,.24,.40),color)
            b.text('GameBoxMark',str(i+1),((i-1)*.46,2.735,3.99),.20,'Coral')
        for x in (-2.55,2.55):plant(b,'TentPlant',x,1.75)
    elif n==19:
        b.box('LandingMat',(0,0,.18),(5.15,4.6,.36),'Blue',collision=True)
        b.cyl('TurntableBase',(0,0,.52),1.43,.47,'Teal',48,collision=True)
        b.cyl('PivotHousing',(0,0,.91),.51,.36,'Metal',32,collision=True)
        verts=[]
        for z in (1.10,1.22):
            for j in range(32):
                a=j*math.tau/32
                verts.append((2.08*math.cos(a),.56*math.sin(a),z+.10*math.cos(a)**4))
        faces=[tuple(range(31,-1,-1)),tuple(range(32,64))]+[(j,(j+1)%32,(j+1)%32+32,j+32) for j in range(32)]
        b.mesh('SurfboardStaticDisplay',verts,faces,'Cream')
        b.box('BoardCenterStripe',(0,0,1.236),(2.90,.16,.016),'Teal')
        for x in (-.72,.72):b.box('FootMark',(x,0,1.252),(.47,.38,.025),'DarkTeal')
        for x in (-2.32,2.32):
            for y in (-1.96,1.96):b.box('SoftBarrierPost',(x,y,.60),(.18,.18,.84),'Teal',collision=True)
            b.beam('SafetyRope',(x,-1.96,.97),(x,1.96,.97),.032,'Cream')
        for y in (-1.96,):b.beam('RearRope',(-2.32,y,.97),(2.32,y,.97),.032,'Cream')
        for x in (-2.17,2.17):
            b.box('NoticePost',(x,.85,1.05),(.1,.1,1.7),'Wood')
            sign(b,'Notice','SURF\nBALANCE',x,.91,1.88,.83,'Teal',.76)
        b.guide('MovingBoardPivot',(0,0,1.06))
        b.guide('RiderPosition',(0,0,1.25))
    elif n==20:
        canopy(b,'MocktailCanopy',5.10,3.85,3.65,'Cream')
        curved_bar(b)
        shelves(b,'BottleDisplay',0,-1.53,1.44,3.90,3)
        for i,x in enumerate((-1.43,-.48,.48,1.43)):
            stool(b,'BarSeat',x,1.28,'Coral')
            b.cyl('MocktailGlass',(x,.60,1.62),.09,.30,'Glass')
            b.cyl('Juice',(x,.60,1.66),.078,.17,['Coral','Amber','Green'][i%3])
            b.cyl('CitrusGarnish',(x+.08,.61,1.81),.07,.016,'Amber',20,(math.pi/2,0,0))
        sign(b,'SunsetHeader','SUNSET MOCKTAIL',0,2.08,3.07,3.65,'Wood')
        for x in (-2.22,2.22):plant(b,'BarPlant',x,1.35)
        b.guide('Bartender',(0,-.86,0))
    elif n==21:
        b.kiosk('Taproom',5.05,3.62,'Teal')
        shelves(b,'Bottles',0,-1.29,1.4,3.9,3)
        sign(b,'TaproomHeader','CRAFT BEER',0,.16,3.25,3.75,'Wood')
        for x in (-1.3,-.65,0,.65,1.3):
            b.curve('TapPipe',[(x,.45,1.20),(x,.45,1.93),(x,.80,1.93),(x,.80,1.77)],.035,'Metal')
            b.box('TapHandle',(x,.44,2.06),(.09,.09,.31),'Wood')
            b.cyl('BeerGlass',(x,.81,1.37),.085,.26,'Amber')
        for x in (-1.62,1.62):
            b.cyl('Barrel',(x,1.62,.58),.49,1.02,'Wood',24,collision=True)
            for z in (.16,.63,1.01):b.torus('BarrelHoop',(x,1.62,z),.49,.033,'Metal',(0,0,0))
            b.cyl('StandingTable',(x,1.62,1.17),.60,.1,'DarkWood')
        b.guide('Bartender',(0,-.15,0))
    elif n==22:
        b.box('DanceDeck',(0,0,.18),(8.50,7.05,.36),'DarkWood',collision=True)
        for x in (-3.87,3.87):
            for y in (-3.06,2.75):b.box('CanopyPost',(x,y,2.28),(.18,.18,4.2),'Wood',collision=True)
        b.mesh('SailShade',[(-4,-3.12,4.35),(4,-3.12,4.35),(4,2.9,4.15),(-4,2.9,4.15),(0,-.1,3.64)],
               [(0,1,4),(1,2,4),(2,3,4),(3,0,4)],'Blue')
        for i in range(6):
            for j in range(5):b.box('DanceFloor',(-2.0+i*.80,-.80+j*.75,.378),(.76,.71,.035),'Pink' if (i+j)%3 else 'Blue',bevel=.006)
        b.box('DjDesk',(0,-2.24,.97),(3.90,.90,1.18),'DarkTeal',collision=True)
        for x in (-.9,.9):b.cyl('Turntable',(x,-2.20,1.60),.30,.04,'Metal')
        b.box('Mixer',(0,-2.20,1.62),(.5,.6,.07),'Teal')
        b.box('Backdrop',(0,-3.10,2.32),(5.00,.1,3.2),'DarkTeal')
        b.box('Screen',(0,-3.035,2.55),(3.9,.035,1.90),'Screen')
        b.text('DiscoScreen','SILENT\nDISCO',(0,-3.0,2.55),.58,'Pink')
        for x in (-3.07,3.07):
            b.box('SpeakerStack',(x,-2.61,1.75),(.80,.75,2.75),'Metal',collision=True)
            for z in (.81,1.60,2.37):b.cyl('SpeakerCone',(x,-2.21,z),.27,.045,'DarkTeal',24,(math.pi/2,0,0))
        b.box('HeadphoneDesk',(3.08,1.50,.92),(1.85,1.1,1.38),'Wood',collision=True)
        sign(b,'Rental','HEADPHONE\nRENTAL',3.05,1.12,2.25,1.8,'Teal',.70)
        for x in (2.49,3.08,3.66):
            b.torus('HeadphoneBand',(x,1.46,1.94),.18,.027,'Metal')
            for dx in (-.18,.18):b.box('HeadphoneEarCup',(x+dx,1.46,1.86),(.075,.10,.18),'Pink')
        for x in (-3.85,3.85):
            plant(b,'ClubPlant',x,-3.05)
            plant(b,'ClubPlant',x,2.75)
        for i in range(9):
            x=-3.4+i*.85
            b.sphere('FestoonBulb',(x,.50,3.75-.28*math.sin(i*math.pi/8)),(.065,.065,.09),'Amber')
        b.curve('FestoonCable',[(-3.85,.5,3.95),(0,.5,3.43),(3.85,.5,3.95)],.015,'Metal')
        b.guide('DjPosition',(0,-2.80,.36))
    elif n==23:
        b.box('RestroomDeck',(0,0,.20),(4.70,4.15,.40),'Wood',collision=True)
        b.box('BackWall',(0,-1.69,1.85),(4.20,.18,2.92),'Wood',collision=True)
        for x in (-2.02,2.02):b.box('SideWall',(x,-.03,1.85),(.18,3.34,2.92),'Wood',collision=True)
        b.box('FrontPier',(0,1.69,1.85),(.32,.19,2.92),'Wood',collision=True)
        for x in (-1.05,1.05):
            simple_door(b,'RestroomDoor',x,1.71,1.37,2.40)
            person_icon(b,'RestroomPictogram',x,1.79,2.03,.63)
        for z in (.68,1.02,1.36,1.70,2.04,2.38,2.72,3.06):
            b.box('RearLouver',(0,-1.80,z),(4.28,.05,.11),'DarkWood')
            for x in (-2.13,2.13):b.box('SideLouver',(x,0,z),(.05,3.45,.11),'DarkWood')
        corners=[(-2.42,-2.10,3.36),(2.42,-2.10,3.36),(2.42,2.10,3.36),(-2.42,2.10,3.36)]
        b.mesh('HipRoof',corners+[(-.62,0,4.23),(.62,0,4.23)],[(0,1,5,4),(1,2,5),(2,3,4,5),(3,0,4)],'DarkWood')
        for i in range(5):
            f=(i+1)/7
            b.box('RoofFrontSlat',(0,2.10*(1-f),3.36+.87*f),(4.80*(1-f)+1.25*f,.07,.065),'Wood')
        b.box('VentTower',(0,0,4.25),(.95,.83,.5),'Cream')
        for z in (4.11,4.24,4.37):b.box('VentSlat',(0,.44,z),(.82,.06,.055),'DarkWood')
        b.mesh('VentRoof',[(-.65,-.58,4.51),(.65,-.58,4.51),(.65,.58,4.51),(-.65,.58,4.51),(0,0,4.75)],[(0,1,4),(1,2,4),(2,3,4),(3,0,4)],'Wood')
        for x in (-2.17,2.17):plant(b,'RestroomPlant',x,1.45,.40)
    elif n==24:
        b.box('AccessibleFloor',(0,0,.07),(3.8,3.8,.14),'Cream',collision=True)
        b.box('RearWall',(0,-1.80,1.52),(3.8,.15,2.9),'White',collision=True)
        b.box('LeftWall',(-1.80,0,1.52),(.15,3.6,2.9),'White',collision=True)
        b.box('RightHalfWall',(1.80,-.90,1.52),(.15,1.8,2.9),'White',collision=True)
        b.box('WideDoorPost',(-1.68,1.64,1.47),(.22,.22,2.8),'Teal',collision=True)
        b.box('WideDoorLintel',(-.61,1.64,2.91),(2.33,.22,.23),'Teal')
        b.box('SlidingDoorParked',(-1.67,.80,1.44),(.10,1.55,2.60),'Cream',collision=True)
        toilet_fixture(b,'AccessibleToilet',-.70,-1.06)
        sink(b,'AccessibleSink',.88,-1.25)
        for x in (-1.21,-.18):b.curve('GrabRail',[(x,-1.47,.68),(x,-.44,.68),(x,-.44,.96)],.035,'Metal')
        b.torus('TurningCircle',(0,.15,.153),.76,.017,'White',(0,0,0))
        b.box('WheelchairSign',(1.807,-.85,1.94),(.025,.59,.69),'Blue')
        b.box('FrontAccessSign',(-1.68,1.79,2.13),(.40,.03,.50),'Blue')
        wheelchair(b,'AccessPictogram',-1.68,1.82,2.12,.43)
        b.guide('AccessibleEntry',(-.45,2.05,0))
        b.guide('TurningCenter',(0,.15,.15))
    elif n==25:
        b.box('FamilyFloor',(0,0,.075),(4.7,3.7,.15),'Cream',collision=True)
        b.box('FamilyBack',(0,-1.76,1.46),(4.70,.16,2.78),'White',collision=True)
        for x in (-2.26,2.26):b.box('FamilySide',(x,0,1.46),(.17,3.50,2.78),'White',collision=True)
        b.box('FrontLeftWing',(-1.79,1.64,1.46),(1.10,.18,2.78),'Cream',collision=True)
        b.box('FrontRightWing',(1.90,1.64,1.46),(.86,.18,2.78),'Cream',collision=True)
        b.box('RoofRear',(0,-.73,2.94),(4.83,2.20,.16),'Cream')
        b.box('EntranceFrame',(0,1.64,2.66),(3.10,.25,.22),'Teal')
        for x in (-1.45,1.45):b.box('EntranceJamb',(x,1.64,1.45),(.15,.25,2.44),'Teal')
        toilet_fixture(b,'AdultToilet',-.83,-1.15)
        toilet_fixture(b,'ChildToilet',.08,-1.15,.62)
        b.box('ChangingTable',(1.25,-.95,.90),(1.03,.73,.15),'Wood')
        b.box('ChangingMat',(1.25,-.95,1.0),(.92,.61,.07),'Cream')
        for x in (.71,1.79):b.box('ChangingGuard',(x,-.95,1.02),(.065,.74,.18),'Teal')
        sink(b,'FamilySink',1.50,.63)
        sign(b,'FamilyHeader','FAMILY RESTROOM',0,1.69,3.10,3.30,'Teal',.62)
        person_icon(b,'AdultIcon',-1.79,1.755,1.86,.70)
        person_icon(b,'ChildIcon',-1.30,1.755,1.65,.42)
        b.guide('FamilyEntry',(0,2.05,0))
    elif n==26:
        b.box('CabinBase',(0,0,.11),(1.5,1.59,.22),'Metal',bevel=.09,collision=True)
        b.box('CabinBack',(0,-.71,1.34),(1.40,.12,2.34),'Teal',bevel=.05,collision=True)
        for x in (-.66,.66):b.box('CabinSide',(x,0,1.34),(.12,1.38,2.34),'Teal',bevel=.06,collision=True)
        b.box('CabinRoof',(0,0,2.48),(1.49,1.58,.28),'Cream',bevel=.12)
        b.sphere('RoundedRoof',(0,0,2.48),(.745,.78,.17),'Cream')
        simple_door(b,'CabinDoor',0,.727,1.07,2.11)
        person_icon(b,'CabinPictogram',0,.801,1.76,.56)
        for x in (-.57,.57):b.box('MouldedDoorRib',(x,.78,1.17),(.033,.03,1.97),'DarkTeal')
        b.box('RoofVent',(0,-.31,2.67),(.44,.36,.14),'White')
        for i in range(4):b.box('VentSlot',(-.13+i*.085,-.31,2.746),(.028,.27,.012),'Metal')
        b.text('CabinBrand','FESTA\nFIESTA',(0,.815,1.28),.15,'Cream')
    elif n==27:
        b.cyl('RoundDeck',(0,0,.09),2.12,.18,'Wood',48,collision=True)
        b.cyl('CounterBase',(0,0,.63),1.73,1.03,'Teal',48,collision=True)
        b.cyl('RoundCounter',(0,0,1.20),1.86,.12,'Wood',48)
        for j in range(16):
            a=j*math.tau/16
            b.box('CounterPanel',(1.73*math.cos(a),1.73*math.sin(a),.70),(.06,.48,.81),'Cream',rotation=(0,0,a))
        for x,y in ((-1.51,-.75),(1.51,-.75),(-1.51,.75),(1.51,.75)):
            b.cyl('RoundCanopyPost',(x,y,1.99),.075,1.69,'Teal',16,collision=True)
        b.cyl('CanopyDisc',(0,0,2.91),2.2,.22,'Teal',48)
        b.cyl('CanopyTop',(0,0,3.06),2.07,.12,'Cream',48)
        b.cyl('LighthouseBody',(0,0,3.67),.45,1.16,'Cream',32)
        for z in (3.30,3.76):b.cyl('LighthouseBand',(0,0,z),.461,.19,'Teal',32)
        b.cyl('LanternRoom',(0,0,4.40),.38,.44,'Glass',24)
        b.cyl('LanternDeck',(0,0,4.16),.53,.13,'Teal',32)
        for j in range(6):
            a=j*math.tau/6;b.beam('LanternMullion',(.37*math.cos(a),.37*math.sin(a),4.18),(.37*math.cos(a),.37*math.sin(a),4.63),.025,'Teal')
        b.mesh('LighthouseCap',[(.55*math.cos(j*math.tau/12),.55*math.sin(j*math.tau/12),4.66) for j in range(12)]+[(0,0,5.10)],[(j,(j+1)%12,12) for j in range(12)],'Teal')
        sign(b,'Information','INFORMATION',0,2.22,2.87,3.32)
        for x in (-1.49,1.49):
            b.beam('InformationSignBrace',(x,.75,2.76),(x,2.18,2.82),.03,'Teal')
        b.cyl('InfoRoundel',(0,1.76,.73),.30,.04,'Teal',32,(math.pi/2,0,0))
        b.text('InfoIcon','i',(0,1.80,.74),.46,'Cream')
        b.box('DeskMap',(-.78,.99,1.29),(.65,.51,.025),'Cream')
        b.box('DeskTerminal',(.70,.7,1.51),(.44,.18,.45),'Screen',rotation=(.14,0,0))
        b.guide('LanternLight',(0,0,4.40))
    elif n==28:
        b.box('KioskFooting',(0,0,.06),(4.24,1.96,.12),'Cream',collision=True)
        for x in (-1.88,1.88):
            b.box('MapPost',(x,0,1.62),(.18,.22,3.12),'Teal',collision=True)
            b.box('MapFoot',(x,0,.16),(.58,.69,.32),'DarkTeal',collision=True)
        b.box('MapFrame',(0,0,1.73),(3.50,.26,1.82),'Wood',collision=True)
        for side in (-1,1):
            yy=side*.145
            b.box('ReplaceableMapPanel',(0,yy,1.73),(3.26,.02,1.56),'Cream')
            for row in range(4):
                for col in range(6):
                    x=-1.23+col*.49;z=1.16+row*.37
                    b.box('MapBlock',(x,yy+side*.018,z),(.27,.015,.18),['Teal','Green','Coral','Sand'][(row+col)%4])
            for z in (1.34,1.72,2.1):b.box('MapWalkway',(0,yy+side*.03,z),(3.0,.018,.035),'White')
            rot=(math.pi/2,0,math.pi) if side>0 else (math.pi/2,0,0)
            b.text('MapTitle','FESTIVAL MAP',(0,yy+side*.035,2.39),.19,'DarkTeal',rot)
            b.guide('MapViewingSide',(0,side*1.4,0))
        for i in range(10):b.box('MapAwning',(-1.91+i*.425,0,3.09),(.43,1.9,.10),'Teal' if i%2==0 else 'Cream',rotation=(.12,0,0))
    elif n==29:
        b.box('ContainerFloor',(0,0,.10),(6.65,3.15,.20),'DarkTeal',collision=True)
        b.box('ContainerRear',(0,-1.48,1.56),(6.55,.14,2.91),'Teal',collision=True)
        for x in (-3.18,3.18):b.box('ContainerSide',(x,0,1.56),(.16,2.95,2.91),'Teal',collision=True)
        b.box('ContainerRoof',(0,0,3.05),(6.80,3.30,.18),'Teal')
        for x in [-3.1+i*.31 for i in range(21)]:b.box('ContainerRoofRib',(x,0,3.16),(.07,3.23,.06),'DarkTeal')
        b.box('CounterWall',(0,1.43,.72),(6.34,.12,1.22),'Teal',collision=True)
        b.box('Counter',(0,1.57,1.39),(6.47,.61,.12),'Wood')
        b.box('WindowHeader',(0,1.45,2.76),(6.46,.14,.39),'Teal')
        for x in (-3.04,-1.04,1.04,3.04):b.box('WindowMullion',(x,1.45,2.04),(.15,.15,1.53),'DarkTeal',collision=True)
        for i,x in enumerate((-2.04,0,2.04)):
            b.box('TicketCounterTerminal',(x,1.17,1.66),(.41,.19,.45),'Screen')
            b.box('WindowNumber',(x,1.56,2.62),(.36,.04,.31),'Cream')
            b.text('WindowNumberText',str(i+1),(x,1.60,2.62),.22,'Teal')
            b.guide('TicketWindow_%d'%i,(x,2.0,0))
        for i in range(12):b.box('TicketAwning',(-3.15+i*.574,1.43,2.84),(.58,1.17,.10),'Teal' if i%2==0 else 'Cream',rotation=(.15,0,0))
        sign(b,'TicketHeader','TICKETS',0,.30,3.53,4.4,'Teal',.70)
        b.box('TicketSymbol',(0,.25,3.98),(1.04,.16,.49),'Cream',rotation=(0,.09,.08))
    elif n==30:
        b.kiosk('LostFound',5.55,3.55)
        sign(b,'LostFoundHeader','LOST CHILD & LOST AND FOUND',0,.25,3.22,4.80,'Teal',.64)
        shelves(b,'StoredBelongings',1.16,-1.31,.86,1.5,4)
        for x in (-1.85,-1.27,-.69):
            b.box('StorageLocker',(x,-1.34,1.76),(.49,.24,1.05),'Cream')
            b.box('LockerHandle',(x+.12,-1.20,1.77),(.04,.03,.15),'Metal')
        b.bench('WaitingBench',0,1.45,2.45)
        b.sphere('ChildSign',( -.41,.33,3.71),(.36,.09,.36),'Cream')
        person_icon(b,'ChildSignIcon',-.41,.43,3.71,.48,'Teal')
        b.box('LuggageSign',(.45,.33,3.68),(.56,.18,.52),'Coral')
        b.curve('LuggageHandle',[(.27,.33,3.94),(.27,.33,4.06),(.63,.33,4.06),(.63,.33,3.94)],.027,'Coral')
        for x in (-2.38,2.38):plant(b,'CenterPlant',x,1.43)
        b.guide('ProtectedWaitingArea',(0,1.12,0))
    elif n==31:
        canopy(b,'LanguageLounge',5.60,4.14,4.17,'Teal',True)
        b.box('ConsultationDesk',(0,-1.1,.68),(3.50,.65,1.13),'Teal',collision=True)
        b.box('ConsultationTop',(0,-1.06,1.30),(3.66,.81,.12),'Wood')
        b.box('ConsultationScreen',(.77,-1.20,1.61),(.51,.13,.43),'Screen')
        for x in (-1.3,-.97,-.64):b.box('MapLeaflets',(x,-1.00,1.48),(.23,.13,.29),'Cream')
        sign(b,'LanguageHeader','MULTILINGUAL INFORMATION',0,2.26,3.10,4.45,'Teal',.62)
        for x in (-1.69,1.69):
            b.bench('ConsultationSeat',x,.57,1.26)
            b.box('SeatCushion',(x,.57,.59),(1.16,.46,.13),'Cream')
        b.table('LoungeCoffeeTable',0,.67,1.3,.74)
        b.cyl('TableFlowerpot',(0,.68,.95),.12,.20,'Cream')
        b.sphere('TablePlant',(0,.68,1.12),(.18,.17,.20),'Green')
        for i,label in enumerate(('HELLO','HOLA','BONJOUR')):
            b.box('LanguageCard',(-1.25+i*1.25,-1.76,2.28),(1.10,.04,.49),'Cream')
            b.text('LanguageText',label,(-1.25+i*1.25,-1.73,2.28),.15,'Teal')
        for x in (-2.40,2.40):plant(b,'LoungePlant',x,1.49)
        b.guide('ConsultationGuest',(0,-.30,0))
    else:
        raise ValueError('Service geometry handles facility numbers 8–31, got %s'%n)

    # Kept as non-exporting metadata guides; the PDA importer supplies actual anchors.
    b.guide('CustomerFront',(0,d*.5+.60,0))
    b.guide('NominalBoundsMax',(w*.5,d*.5,h))
