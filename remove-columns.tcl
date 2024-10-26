#!/usr/bin/tclsh

package require dbf

if {$argc > 0} {
	set input_file [lindex $argv 0]

	set delete_list {}

	if {$argc > 1} {
		for {set i 0} {$i < $argc} {incr i} {
			lappend delete_list [lindex $argv $i]
			}
		}

	if {[dbf d -open $input_file -readonly]} {

		set nr [lindex [$d info] 0]
		set nf [lindex [$d info] 1]

		set field_list [$d fields]

		set keep {}

		if {[dbf dd -create temp.dbf]} {
			foreach f $field_list {
				set label [lindex $f 0]
				set type  [lindex $f 1]
				set width [lindex $f 3]
				set prec  [lindex $f 4]
				if {[lsearch $delete_list $label] == -1} {
					lappend keep 1

					if {[string equal $type "Double"]} {
						set w 0
						set p 0

						set max_l 0
						set max_r 0

						foreach value [$d values $label] {
							set value [format "%.${prec}f" $value]
							if {[string first . $value] > 0} {
								set value [string trimright $value 0]
								set value [string trimright $value .]
								}
							set mm [string length $value]
							# If no decimal point, l is width of the text, r is zero
							set l $mm
							set r 0
							# If there is a decimal point, l is position of decimal point, r is length of string after decimal point
							set k [string first . $value]
							if {$k >= 0} {
								set l $k
								incr k
								set m [expr $mm - $k]
								if {$m > $r} {
									set r $m
									}
								}
							if {$l > $max_l} {
								set max_l $l
								}
							if {$r > $max_r} {
								set max_r $r
								}
							set w [expr $max_l + 1 + $max_r]
							set p $max_r
							}

						if {$w != $width || $p != $prec} {
						#	puts "For $label"
						#	puts "  old width $width"
						#	puts "  new width $w"
						#	puts "  old prec $prec"
						#	puts "  new prec $p"

							set width $w
							set prec $p
							}
						}

					# puts "$dd add $label $type $width $prec"
					$dd add $label $type $width $prec
					} \
				else {
					lappend keep 0
					}
				}


			for {set i 0} {$i < $nr} {incr i} {
				set row [$d record $i]
				set data ""
				for {set j 0} {$j < $nf} {incr j} {
					if {[lindex $keep $j]} {
						set f [lindex $field_list $j]
						set t [lindex $f 1]
						set p [lindex $f 4]
						set value [lindex $row $j]
						if {[string equal $t "Integer"] || [string equal $t "Double"]} {
							set value [format "%.${p}f" $value]
							if {[string first . $value] > 0} {
								set value [string trimright $value 0]
								set value [string trimright $value .]
								}
							}
						lappend data $value
						}
					}
				$dd insert end $data
				}

			$dd close
			}

		$d close
		}
	} \
else {
	puts "Name the input file on the command line"
	}
