#!/usr/bin/tclsh

package require dbf

set file_name "test6.dbf"

if {[dbf d -create $file_name]} {
	$d add NAME String 12
	$d add VALUE Double 8 5
	$d add Q Logical 1
	$d add NUM Integer 4
	$d insert 0 "First one" 1.23 T 123
	$d insert 1 "Second one" 4.56 F 456
	$d insert 2 "Third one" 7.89 T 789
    $d deleted 1 true
	$d close

	if {[dbf d -open $file_name]} {
		set nr [lindex [$d info] 0]
		set nf [lindex [$d info] 1]
		puts "Number of records: $nr"
		puts "Number of fields:  $nf"
		puts "Fields:"
		foreach item [$d fields] {
			puts "[format "  %10s %10s %s %4d %2d" [lindex $item 0] [lindex $item 1] [lindex $item 2] [lindex $item 3] [lindex $item 4]]"
			}
		for {set i 0} {$i < $nr} {incr i} {
			puts "[$d record $i]  deleted:[$d deleted $i]"
			}
		$d forget
		} \
	else {
		puts "Error: could not open recently-created file $file_name"
		}
	} \
else {
	puts "Error: could not create file $file_name"
	}

