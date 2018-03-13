use <utils/build_plate.scad>

//-------------------------------------------------------
//WirelessPalaSensor Box V1.2
//-------------------------------------------------------

_part = "both"; // [box, lid, both]

_innerDimensions = true;


//PCB dimensions
_pcbX = 45.085;
_pcbY = 30.48;

// width of the box (affects the length of the hinges)
_width = 46;

// length of the box
_length = 31.5;

// height of the box
_height = 19;

// rounding radius of the corners of the box
_rounding = 4;

// thickness of the walls around the magnets
_minimumWallThickness = 0.5;

// thickness of the side walls without the hinge
_sidewallThickness = 1.5;

// thickness of the bottom of the box or the top of the lid
_horizontalThickness = 1.25;

// diameter of magnets in the hinge
_magnetDiameter = 2.5;

// height magnets in the hinge
_magnetHeight = 1;

// gap bewteen the lid and box parts
_hingeGap = 0.4;

// Size of lid's spheres that go into box holes
_hingeOverlap = 0.15;

_lidRadius = 0;

module roundRect(width, depth, height, round) {
	round = min(round, width/2);
	linear_extrude(height=height,center=true) offset(r=round) square([width-2*round,depth-2*round],center=true);
}

module roundRectFillet(width, depth, height, round, radius) {
    
    radius=max(0.00001,radius);
    
    minkowski(){
        union(){
            
            translate([0,0,(height-radius)/2])cylinder(r=radius,h=height-radius,center=true);
                
            translate([0,0,height-(radius>height?height:radius)])
            resize([0,0,radius>height?height:0])
            difference(){
                sphere(r=radius, center=true);
                translate([0,0,-radius/2]) cube([2*radius,2*radius,radius],center=true);
            }
        }
    
        roundRect(width-(2*radius),depth-(2*radius),0.000001,round);
    }
}

module makeBase(width, depth, height, rounding, minimum, side, bottom, mdiameter, mheight, gap) {
	eps = 0.1;
	iwidth = width - side*2;
	idepth = depth - mdiameter*2 - minimum*4;
	hingeCutZ = mdiameter + minimum*2/* + gap*/;
	hingeCutWidth = width - rounding*2 - mheight*2;
	fillet = mdiameter/2 + minimum;

    union(){
        difference() {
            translate([0,0,height/2]) 
            difference() {
                roundRect(width, depth, height, rounding);
                translate([0,0,bottom]) {
                    //roundRect(iwidth, idepth, height, rounding-side);
                    cube(size=[iwidth, idepth, height],center=true);
                }
            }
            
    
            // hinge cutout
            translate([0,0,height - hingeCutZ/2 + eps/2]) {
                cube(size=[hingeCutWidth, depth+eps, hingeCutZ + eps], center=true);
            }
            
            // sphere cutout
            for (x=[-1,1])
            for (y=[-1,1]) {
                translate([x * (hingeCutWidth/2 - eps/2), y*(depth/2 - minimum - mdiameter/2), height - minimum - mdiameter/2])
                sphere(d=mdiameter);
            }
            

            //WirelessPalaSensor bornier Cutout
            //bornier2
            translate([iwidth/2,-4-0.5+0.635,bottom+4/*4mm from bottom*/])cube([(width-iwidth)/2,4,6]);
            translate([iwidth/2,+0.5+0.635,bottom+4/*4mm from bottom*/])cube([(width-iwidth)/2,4,6]);

            //Micro-USB
            //Widths : 8.8 and 5.9
            //heights : 3.8 and 1.8
            translate([-width/2,2.14,1.9+bottom+2+0.75])rotate([90,0,90])linear_extrude((width-iwidth)/2)polygon([[-4.4,1.9],[4.4,1.9],[4.4,-0.1],[2.95,-1.9],[-2.95,-1.9],[-4.4,-0.1],[-4.4,1.9]]);
            //translate([-width/2,-8.8/2+2.14,bottom+2+0.75])cube([(width-iwidth)/2,8.8,3.8]);

            //DS18B20 wire
            translate([-iwidth/2,-idepth/2+2.54,height])rotate([0,-90,0])cylinder(d=2,h=(width-iwidth)/2);

            //WirelessPalaSensor Texts
            //bornier2
            translate([width/2-0.5,0.635,1])rotate([90,0,90])linear_extrude(0.6)text(text="PALA",font="Liberation Sans:style=Bold",size=3.5,halign="center");
            translate([width/2-0.5,0.635-7.5,7.2])rotate([90,0,90])linear_extrude(0.6)text(text="-",font="Liberation Sans:style=Bold",size=6,halign="center");
            translate([width/2-0.5,0.635+7.5,7])rotate([90,0,90])linear_extrude(0.6)text(text="+",font="Liberation Sans:style=Bold",size=6,halign="center");

            //Micro-USB
            translate([-width/2+0.5,2.14,9])rotate([90,0,-90])linear_extrude(0.6)text(text="PWR",font="Liberation Sans:style=Bold",size=3.5,halign="center");

        }
        
        //WirelessPalaSensor additions
        //Inside supports
        translate([-_pcbX/2+3,_pcbY/2-7,bottom])cube([3,7,2]);
        translate([-_pcbX/2+3,-_pcbY/2+4.445,bottom])cube([3,7,2]);
        translate([iwidth/2,-idepth/2,bottom])linear_extrude(2)polygon([[0,0],[0,6.35],[-6.35,0]]);
        translate([iwidth/2,idepth/2,bottom])linear_extrude(2)polygon([[0,0],[0,-6.35],[-6.35,0]]);
        
    }
}


module makeLid(width, depth, rounding, minimum, side, bottom, mdiameter, mheight, gap, overlap) {
	eps = 0.1;
	hingeWidth = width - rounding*2 - mheight*2 - gap*2;
	hingeSize = mdiameter + minimum*2;
    
    //WirelessPalaSensor
    idepth = depth - mdiameter*2 - minimum*4;
	
	difference() {
		union() {
			translate([0,0,bottom/2]) {
				roundRect(width, depth, bottom, rounding);
			}	

			// hinges
			for (s=[-1,1]) {
				translate([0,s*(depth/2 - hingeSize/2),bottom + hingeSize/2]) {
					hull() {
						rotate([0,90,0]) {
							cylinder(r=hingeSize/2, h=hingeWidth,center=true);
						}
						translate([0,s*-hingeSize/4,0]) {
							cube(size=[hingeWidth, hingeSize/2, hingeSize], center=true);
						}
						translate([0,0,-hingeSize/4-eps]) {
							cube(size=[hingeWidth, hingeSize, hingeSize/2], center=true);
						}
					}
				}
			}
            // sphere additions
            for (x=[-1,1])
            for (y=[-1,1]) {
                translate([x * (hingeWidth/2 - eps), y*(depth/2 - minimum - mdiameter/2), mdiameter/2 + bottom + minimum]){
                    difference(){
                        sphere(d=mdiameter);
                        translate([x*(mdiameter/2+eps+gap+overlap),0,0]) cube([mdiameter,mdiameter,mdiameter],center=true);
                    }
                }
            }
            
            //WirelessPalaSensor Additions
            //buttons holes
            translate([_pcbX/2-4.52,_pcbY/2-3.655,0])cylinder(d=6.5,h=10.5+bottom);
            translate([_pcbX/2-4.37,-_pcbY/2+8.1,0])cylinder(d=6.5,h=10.5+bottom);
		}
        
        //WirelessPalaSensor cutouts
        //buttons holes
        translate([_pcbX/2-4.52,_pcbY/2-3.655,0])cylinder(d=4,h=10.5+bottom);
        translate([_pcbX/2-4.37,-_pcbY/2+8.1,0])cylinder(d=4,h=10.5+bottom);
        
        //bornier2
        translate([-_pcbX/2+3.8,4.5/2+0.5+0.635,0])cylinder(d=4.5,h=bottom);
        translate([-_pcbX/2+3.8,-4.5/2-0.5+0.635,0])cylinder(d=4.5,h=bottom);
        
        //ESP leds
        translate([-7.5,-6.24,0.25])cylinder(d=1.5,h=bottom-0.25);

        //RGB Led
        translate([-_pcbX/2+3.8,_pcbY/2-3.8,0.5])cylinder(d=3,h=bottom-0.5);
        translate([-_pcbX/2+3.8,_pcbY/2-3.8,0.25])cylinder(d=1.5,h=bottom-0.25);
        
        //WirelessPalaSensor Texts
        //button holes Texts
        
        translate([_pcbX/2-8.5,11,0.5])rotate([0,-180,90])difference(){
            linear_extrude(0.6)offset(r=0.6)text(text="RST",font="Liberation Sans:style=Bold",size=3,halign="center");
            linear_extrude(0.6)text(text="RST",font="Liberation Sans:style=Bold",size=3,halign="center");
        }
        translate([_pcbX/2-8.5,-6.66,0.5])rotate([0,-180,90])difference(){
            linear_extrude(0.6)offset(r=0.6)text(text="RSC",font="Liberation Sans:style=Bold",size=3,halign="center");
            linear_extrude(0.6)text(text="RSC",font="Liberation Sans:style=Bold",size=3,halign="center");
        }
        
        //translate([15,11,0.5])rotate([0,180,90])linear_extrude(0.6)text(text="RST",font="Liberation Sans:style=Bold",size=3,halign="center");
        //translate([15,-6.66,0.5])rotate([0,180,90])linear_extrude(0.6)text(text="RSC",font="Liberation Sans:style=Bold",size=3,halign="center");
        
        
        //leds
        
        translate([-6,0,0.5])rotate([0,-180,90])difference(){
            linear_extrude(0.6)offset(r=0.5)text(text="ACT",font="Liberation Sans:style=Bold",size=3,halign="center");
            linear_extrude(0.6)text(text="ACT",font="Liberation Sans:style=Bold",size=3,halign="center");
        }
        //translate([-6,0.5,0.5])rotate([0,-180,90])linear_extrude(0.6)text(text="ACT",font="Liberation Sans:style=Bold",size=3,halign="center");
        
        
        //Brand :-)
        
        translate([-6,-13,0.5])rotate([0,-180,180])difference(){
            linear_extrude(0.6)offset(r=0.6)text(text="WirelessPalaSensor",font="Liberation Sans:style=Bold",size=2.4,halign="center");
            linear_extrude(0.6)text(text="WirelessPalaSensor",font="Liberation Sans:style=Bold",size=2.4,halign="center");
        }
        translate([2,-8.5,0.5])rotate([0,-180,180])difference(){
            linear_extrude(0.6)offset(r=0.6)text(text="By J6B",font="Liberation Sans:style=Bold",size=2.4,halign="center");
            linear_extrude(0.6)text(text="By J6B",font="Liberation Sans:style=Bold",size=2.4,halign="center");
        }
        //translate([-6,-13,0.5])rotate([0,-180,180])linear_extrude(0.6)text(text="WirelessPalaSensor",font="Liberation Sans:style=Bold",size=2.4,halign="center");
        //translate([2,-8.5,0.5])rotate([0,-180,180])linear_extrude(0.6)text(text="By J6B",font="Liberation Sans:style=Bold",size=2.4,halign="center");
	}

}

module make($fn=60) {
	// minimal error checking
	eps = 0.1;
    
	rounding = max(_rounding, _sidewallThickness + eps);
	height = max(_height, _horizontalThickness + _magnetDiameter + _minimumWallThickness*2 + _hingeGap);


    //if innerDimensions then recalculate
    if(_innerDimensions == true){
        
        width=_width+2*_sidewallThickness;
        length=_length+2*(_magnetDiameter+2*_minimumWallThickness);
        height=height+_horizontalThickness;
        
        if (_part == "box" || _part == "both") {
            makeBase(width, length, height, rounding, 
                        _minimumWallThickness, _sidewallThickness, _horizontalThickness, 
                        _magnetDiameter, _magnetHeight, _hingeGap);
        }
        if (_part == "lid" || _part == "both") {
            translate([0,0,height+_horizontalThickness+0.1]) rotate([0,180,0]) makeLid(width, length, rounding, 
                        _minimumWallThickness, _sidewallThickness, _horizontalThickness, 
                        _magnetDiameter, _magnetHeight, _hingeGap, _hingeOverlap);
        }
    }
    else{
        if (_part == "box" || _part == "both") {
            makeBase(_width, _length, height, rounding, 
                        _minimumWallThickness, _sidewallThickness, _horizontalThickness, 
                        _magnetDiameter, _magnetHeight, _hingeGap);
        }
        if (_part == "lid" || _part == "both") {
            translate([0,0,height+_horizontalThickness+0.1]) rotate([0,180,0]) makeLid(_width, _length, rounding, 
                        _minimumWallThickness, _sidewallThickness, _horizontalThickness, 
                        _magnetDiameter, _magnetHeight, _hingeGap, _hingeOverlap);
        }

		
	}

}

build_plate(0);
make();

