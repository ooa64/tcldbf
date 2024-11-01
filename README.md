# tcldbf
A Tcl interface to read DBF files - https://geology.usgs.gov/tools/metadata/tcldbf

tcldbf is an extension of Tcl/Tk that provides the Tcl script writer some ability to read and write DBF files. Its core is a set of C wrappers that read dBase files using Frank Warmerdam's shapelib and make the results available to Tcl procedures.

tcldbf-1.3 by Peter N. Schweitzer (U.S. Geological Survey, Reston VA 20192)

tcldbf-1.3.1
	 
	added codepage 1..255 support (decode/encode chars on read/write)
	added deleted command (query or set deleted mark)
	using nativetypes in the add command

Commands summary

	dbf d -open $input_file [-readonly]
 		opens dbase file, returns a handle.
	dbf d -create $input_file [-codepage $codepage]
 		creates dbase file, returns a handle

	info
 		returns {record_count field_count}
 
	codepage
 		returns database codepage
 
	add label type width [prec]
 		adds field specified to the dbf, if created and empty
 
	fields
 		returns a list of lists, each of which consists of
 		{name type native-type width prec}
 
	values $name
 		returns a list of values of the field $name
 
	record $rowid
 		returns a list of cell values (as strings) for the given row
 
	insert $rowid  end value0 [... value1 value2 ...]
 		inserts the specified values into the given record 
 
	update $rowid $field $value
 		replaces the specified values of a single field in the record 
 
	deleted $rowid [true|false]
 		returns or sets the deleted flag for the given rowid
 
	forget
 		closes dbase file
 
Codepage is a string like "LDID/87". Other numbers:

	1 - US MS-DOS
	2 - International MS-DOS
	3 - Windows ANSI Latin I
	4 - Standard Macintosh
	8 - Danish OEM
	9 - Dutch OEM
	10 - Dutch OEM*
	11 - Finnish OEM
	13 - French OEM
	14 - French OEM*
	15 - German OEM
	16 - German OEM*
	17 - Italian OEM
	18 - Italian OEM*
	19 - Japanese Shift-JIS
	20 - Spanish OEM*
	21 - Swedish OEM
	22 - Swedish OEM*
	23 - Norwegian OEM
	24 - Spanish OEM
	25 - English OEM (Great Britain)
	26 - English OEM (Great Britain)*
	27 - English OEM (US)
	28 - French OEM (Canada)
	29 - French OEM*
	31 - Czech OEM
	34 - Hungarian OEM
	35 - Polish OEM
	36 - Portuguese OEM
	37 - Portuguese OEM*
	38 - Russian OEM
	55 - English OEM (US)*
	64 - Romanian OEM
	77 - Chinese GBK (PRC)
	78 - Korean (ANSI/OEM)
	79 - Chinese Big5 (Taiwan)
	80 - Thai (ANSI/OEM)
	87 - Current ANSI CP ANSI
	88 - Western European ANSI
	89 - Spanish ANSI
	100 - Eastern European MS-DOS
	101 - Russian MS-DOS
	102 - Nordic MS-DOS
	103 - Icelandic MS-DOS
	104 - Kamenicky (Czech) MS-DOS 895
	105 - Mazovia (Polish) MS-DOS 620
	106 - Greek MS-DOS (437G)
	107 - Turkish MS-DOS
	108 - French-Canadian MS-DOS
	120 - Taiwan Big 5
	121 - Hangul (Wansung)
	122 - PRC GBK
	123 - Japanese Shift-JIS
	124 - Thai Windows/MS–DOS
	134 - Greek OEM
	135 - Slovenian OEM
	136 - Turkish OEM
	150 - Russian Macintosh
	151 - Eastern European Macintosh
	152 - Greek Macintosh
	200 - Eastern European Windows
	201 - Russian Windows
	202 - Turkish Windows
	203 - Greek Windows
	204 - Baltic Windows
