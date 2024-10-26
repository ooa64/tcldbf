#!/usr/bin/tclsh

package require dbf

set file_name "test5.dbf"

# default codepage "LDID/87" ( 87 - system ANSI, 38 - cp866, 201 - cp1251 ) */

if {[dbf d -create $file_name -codepage "LDID/38"]} {
	$d add NAME String 12
	$d add VALUE Double 8 5
	$d add Q Logical 1
	$d add NUM Integer 4
	$d insert 0 "Перший нах" 1.23 T 123
	$d insert 1 "Другий нах" 4.56 F 456
	$d insert 2 "Третій нах" 7.89 T 789
	$d close

	if {[dbf d -open $file_name]} {
		set nr [lindex [$d info] 0]
		set nf [lindex [$d info] 1]
		puts "Code page: [$d codepage]"
		puts "Number of records: $nr"
		puts "Number of fields:  $nf"
		puts "Fields:"
		foreach item [$d fields] {
			puts "[format "  %10s %10s %s %4d %2d" [lindex $item 0] [lindex $item 1] [lindex $item 2] [lindex $item 3] [lindex $item 4]]"
			}
		for {set i 0} {$i < $nr} {incr i} {
			puts "[$d record $i]"
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

