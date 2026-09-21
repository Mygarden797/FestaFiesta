"""Reference-led silhouettes for the first seven coastal food facilities."""
import math


def cup(b,name,x,y,z,r=.16,h=.32,mat='Cream'):
    b.cyl(name,(x,y,z+h/2),r,h,mat)
    b.cyl(name+'Drink',(x,y,z+h+.003),r*.83,.012,'DarkWood')
    b.torus(name+'Rim',(x,y,z+h),r*.94,.016,mat,(0,0,0))
    b.torus(name+'Handle',(x+r,y,z+h*.57),r*.60,r*.17,mat)


def menu(b,x,y,z,title='MENU'):
    b.box('MenuFrame',(x,y,z),(.59,.08,.84),'Wood')
    b.box('MenuBoard',(x,y+.049,z),(.51,.017,.75),'Metal')
    b.text('MenuTitle',title,(x,y+.062,z+.23),.12)
    for i in range(4):b.box('MenuLine',(x-.035,y+.063,z+.05-i*.12),(.33,.008,.017),'Cream',bevel=0)


def build(b,entry):
    n=entry['number']
    if n in (1,2,6):
        accent={1:'Teal',2:'Coral',6:'Green'}[n]
        b.truck('Truck',accent)
        b.box('RoofSignPlinth',(.05,0,3.04),(2.6,.85,.30),accent)
        b.text('Identity',{1:'COFFEE',2:'FRUIT AID',6:'VEGAN'}[n],(.05,.438,3.06),.25)
        if n==1:
            cup(b,'GiantCoffeeCup',.05,0,3.18,.53,.82)
            b.text('CupBrand','FF',(.05,.532,3.64),.26,'Teal')
            b.box('CoffeeMachine',(.3,.78,1.60),(.83,.47,.54),'Metal')
            b.box('MachineTop',(.3,.77,1.90),(.89,.49,.12),'Cream')
            for x in (.05,.48):
                b.cyl('EspressoDial',(x,1.024,1.7),.065,.04,'Cream',20,(math.pi/2,0,0))
                b.beam('Portafilter',(x,1.03,1.54),(x,1.25,1.54),.035,'Metal')
                cup(b,'EspressoCup',x,1.10,1.38,.075,.11)
            for i in range(3):cup(b,'DisplayCup',-1.4+i*.32,1.26,1.38,.10,.23)
            b.box('StirrerPot',(-.51,1.2,1.55),(.19,.2,.35),'Wood')
            for i in range(5):b.beam('Stirrer',(-.58+i*.035,1.2,1.55),(-.58+i*.035,1.2,1.88),.008,'DarkWood')
            menu(b,1.01,1.28,1.8,'COFFEE')
        elif n==2:
            for i,(x,mat) in enumerate([(-.73,'Coral'),(0,'Green'),(.73,'Amber')]):
                b.sphere('OversizedFruit',(x,0,3.64),(.48,.43,.48),mat)
                b.sphere('FruitLeaf',(x+.15,0,4.05),(.25,.09,.11),'Green')
                b.cyl('Dispenser',(x,.78,1.71),.23,.63,'Glass')
                b.cyl('Juice',(x,.78,1.64),.222,.39,mat)
                b.cyl('DispenserLid',(x,.78,2.04),.25,.065,'Cream')
                b.beam('DispenserTap',(x,1.0,1.56),(x,1.15,1.56),.035,'Metal')
                cup(b,'FruitCup',x,1.32,1.39,.08,.22,'White')
            menu(b,1.17,1.25,1.8,'FRESH')
        else:
            # Large diagonal sandwich cross-section over the shared vehicle shell.
            for z in (3.33,3.83):
                b.mesh('SandwichBread',[(-.8,-.4,z),(.8,-.4,z),(-.8,.6,z),(-.8,-.4,z+.13),(.8,-.4,z+.13),(-.8,.6,z+.13)],[(0,2,1),(3,4,5),(0,1,4,3),(1,2,5,4),(2,0,3,5)],'Cream')
            for i,mat in enumerate(['Green','Coral','Sand','Green']):
                b.box('SandwichLayer',(-.18,-.02,3.51+i*.07),(1.15,.70,.10),mat)
            for x in (-1.15,-.4,.35):
                b.box('ServingTray',(x,1.14,1.40),(.60,.44,.06),'Cream')
                b.box('Sandwich',(x,1.14,1.52),(.36,.28,.18),'Sand')
                b.box('Salad',(x,1.14,1.58),(.39,.29,.04),'Green')
            menu(b,1.03,1.24,1.8,'VEGAN')
    else:
        accent={3:'Teal',4:'Coral',5:'DarkTeal',7:'Teal'}[n]
        b.kiosk('MarketStall',4.4,2.7,accent)
        b.text('Identity',{3:'BUSAN EOMUK',4:'HOTTEOK',5:'CHARCOAL',7:'CLOUD CANDY'}[n],(0,-.197,2.88),.22)
        # Wood slats give the counter a material rhythm without tiny surface geometry.
        for i in range(10):b.box('CounterSlat',(-1.72+i*.38,.997,.62),(.30,.025,.84),'Wood',bevel=.012)
        if n==3:
            b.box('BrothTray',(-.55,.72,1.35),(1.9,.70,.3),'Metal')
            b.box('SoupSurface',(-.55,.72,1.509),(1.72,.52,.015),'Sand',bevel=0)
            for x in (-1.18,-.78,-.38,.02):
                b.beam('Skewer',(x,.62,1.5),(x,.69,2.00),.012,'DarkWood')
                for z in (1.62,1.73,1.84):b.box('FishcakeFold',(x,.66,z),(.20,.10,.065),'Cream',rotation=(0,0,.23))
            for x in (-1.7,1.7):
                b.cyl('Lantern',(x,1.08,2.14),.17,.43,'Amber',16)
                b.cyl('LanternCap',(x,1.08,2.38),.15,.06,'Metal')
            menu(b,1.13,.92,1.59,'EOMUK')
            b.guide('FX_Steam',(-.55,.72,1.57))
        elif n==4:
            b.cyl('RoundGriddle',(-.55,.76,1.27),.67,.14,'Metal',40)
            for x,y in [(-.87,.53),(-.32,.57),(-.80,.94),(-.25,.98)]:
                b.cyl('Hotteok',(x,y,1.37),.20,.07,'Sand')
                b.cyl('GoldenCentre',(x,y,1.409),.135,.005,'Wood')
            b.cyl('PressPlate',(.85,.70,1.28),.20,.04,'Metal')
            b.beam('PressHandle',(.85,.70,1.30),(.85,.70,1.56),.025,'DarkWood')
            b.cyl('GiantHotteok',(0,-.26,3.37),.42,.14,'Sand',32,(math.pi/2,0,0))
            for i in range(3):cup(b,'PaperCup',1.1+i*.24,1.01,1.24,.08,.23,'White')
        elif n==5:
            b.box('LongGrill',(-.15,.7,1.31),(2.9,.71,.27),'Metal')
            b.box('CharcoalBed',(-.15,.7,1.455),(2.70,.51,.02),'Coral')
            for i in range(12):
                x=-1.40+i*.23
                b.beam('Grate',(x,.4,1.48),(x,.99,1.48),.013,'Metal')
                b.beam('Skewer',(x,.34,1.56),(x,1.17,1.56),.01,'Wood')
                for y in (.50,.69,.88):b.box('Yakitori',(x,y,1.57),(.15,.14,.13),'Sand',bevel=.035)
            b.box('Chimney',(1.43,-.74,2.22),(.33,.37,1.67),'Metal')
            b.guide('FX_CharcoalSmoke',(-.15,.7,1.65))
        else:
            for x,z,r in [(-1.30,3.08,.44),(-.73,3.3,.55),(0,3.44,.64),(.74,3.31,.55),(1.32,3.1,.44)]:
                b.sphere('CloudCanopy',(x,-.08,z),(r,.52,r*.67),'White')
            b.cyl('CottonCandyBowl',(-.42,.76,1.28),.53,.24,'Metal',32)
            b.cyl('SpinningBasin',(-.42,.76,1.415),.44,.025,'Pink')
            b.cyl('Spinner',(-.42,.76,1.48),.11,.11,'Cream')
            for i,mat in enumerate(['Pink','White','Blue']):
                x=.6+i*.39
                b.beam('CandyStick',(x,.82,1.23),(x,.82,1.96),.014,'Wood')
                b.sphere('CandyFloss',(x,.82,1.99),(.21,.20,.29),mat)
            b.guide('FX_CottonCandySpinner',(-.42,.76,1.45))
    b.guide('ServiceCounter',(0,1.85,0))
