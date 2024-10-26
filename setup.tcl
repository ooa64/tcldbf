#!/usr/bin/tclsh

set need [list dbfopen.c shapefil.h safileio.c]

set found {}
set links {}

if {[catch "glob ../shapelib-*" d] == 0} {
	set dir_list [lsort -dictionary -decreasing $d]
	foreach d $dir_list {
		foreach f $need {
			if {[file exists $d/$f]} {
				if {[catch {file link -symbolic $f $d/$f} name] == 0} {
					lappend found $f
					lappend links $name
					}
				}
			}
		}
	set ok 1
	foreach f $need {
		if {[lsearch $found $f] == -1} {
			puts "Error: did not find $f"
			set ok 0
			}
		}
	if {$ok} {
		puts "OK: Symbolic links created:"
		foreach name $links {
			puts "  $name"
			}
		}
	} \
else {
	puts "You will need shapelib sources to compile this software"
	puts "These can be obtained at http://shapelib.maptools.org/"
	}
