/*----------------------------------------------------------------------*\
 | C routines that read dBase files using Frank Warmerdam's shapelib	|
 | and make the results available to Tcl procedures.					|
 |	http://pobox.com/~warmerdam											|
 |																		|
 | What do I want to do with dbf files in Tcl?							|
 |																		|
 | dbf d -open $input_file [-readonly]									|
 |		opens dbase file, returns a handle.								|
 | dbf d -create $input_file [-codepage $codepage]						|
 |		creates dbase file, returns a handle							|
 |																		|
 | $d info																|
 |		returns {record_count field_count}								|
 |																		|
 | $d codepage															|
 |		returns database codepage										|
 |																		|
 | $d add label type|nativetype width [prec]							|
 |		adds field specified to the dbf, if created and empty			|
 |																		|
 | $d fields															|
 |		returns a list of lists, each of which consists of				|
 |		{name type native-type width prec}								|
 |																		|
 | $d values $name														|
 |		returns a list of values of the field $name						|
 |																		|
 | $d record $rowid														|
 |		returns a list of cell values (as strings) for the given row	|
 |																		|
 | $d insert $rowid | end value0 [... value1 value2 ...]				|
 |		inserts the specified values into the given record 				|
 |																		|
 | $d update $rowid $field $value										|
 |		replaces the specified values of a single field in the record 	|
 |																		|
 | $d deleted $rowid [true|false]										|
 |		returns or sets the deleted flag for the given rowid			|
 |																		|
 | $d forget															|
 |		closes dbase file												|
 |																		|
 | Peter N. Schweitzer (U.S. Geological Survey, Reston VA 20192)		|
\*----------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <tcl.h>

#include "dbf.h"

#include <shapefil.h>

struct field_info {
	char name [16];
	DBFFieldType type;
	char native;
	int width;
	int precision;
	};

struct dbf_info {
	DBFHandle df;
	Tcl_Encoding enc;
	};

static char *type_of (DBFFieldType t) {
	if (t == FTString ) return ("String" );
	if (t == FTInteger) return ("Integer");
	if (t == FTDouble ) return ("Double" );
	if (t == FTLogical) return ("Logical");
	if (t == FTDate)    return ("Date");
	if (t == FTInvalid) return ("Invalid");
	return ("Unknown");
	}

static DBFFieldType get_type (char *name) {
	DBFFieldType result = FTInvalid;
	if (name && *name) {
		if (name[1] == '\0') {
			if (name[0] == 'C') result = FTString;
			if (name[0] == 'N') result = FTDouble;
			if (name[0] == 'L') result = FTLogical;
			if (name[0] == 'D') result = FTDate;
			}
		else {
			if (strcmp (name,"String" ) == 0) result = FTString;
			if (strcmp (name,"Integer") == 0) result = FTInteger;
			if (strcmp (name,"Double" ) == 0) result = FTDouble;
			if (strcmp (name,"Logical") == 0) result = FTLogical;
			if (strcmp (name,"Date") == 0)    result = FTDate;
			}
		}
	return (result);
	}

static char * codepages[] = {
	NULL,
	"cp437", /* 1 - US MS-DOS */
	"cp850", /* 2 - International MS-DOS */
	"cp1252", /* 3 - Windows ANSI Latin I */
	"macCentEuro", /* 4 - Standard Macintosh */
	NULL,NULL,NULL,
	"cp865", /* 8 - Danish OEM */
	"cp437", /* 9 - Dutch OEM */
	"cp850", /* 10 - Dutch OEM* */
	"cp437", /* 11 - Finnish OEM */
	NULL,
	"cp437", /* 13 - French OEM */
	"cp850", /* 14 - French OEM* */
	"cp437", /* 15 - German OEM */
	"cp850", /* 16 - German OEM* */
	"cp437", /* 17 - Italian OEM */
	"cp850", /* 18 - Italian OEM* */
	"cp932", /* 19 - Japanese Shift-JIS */
	"cp850", /* 20 - Spanish OEM* */
	"cp437", /* 21 - Swedish OEM */
	"cp850", /* 22 - Swedish OEM* */
	"cp865", /* 23 - Norwegian OEM */
	"cp437", /* 24 - Spanish OEM */
	"cp437", /* 25 - English OEM (Great Britain) */
	"cp850", /* 26 - English OEM (Great Britain)* */
	"cp437", /* 27 - English OEM (US) */
	"cp863", /* 28 - French OEM (Canada) */
	"cp850", /* 29 - French OEM* */
	NULL,
	"cp852", /* 31 - Czech OEM */
	NULL,NULL,
	"cp852", /* 34 - Hungarian OEM */
	"cp852", /* 35 - Polish OEM */
	"cp860", /* 36 - Portuguese OEM */
	"cp850", /* 37 - Portuguese OEM* */
	"cp866", /* 38 - Russian OEM */
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
	"cp850", /* 55 - English OEM (US)* */
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
	"cp852", /* 64 - Romanian OEM */
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
	"cp936", /* 77 - Chinese GBK (PRC) */
	"cp949", /* 78 - Korean (ANSI/OEM) */
	"cp950", /* 79 - Chinese Big5 (Taiwan) */
	"cp874", /* 80 - Thai (ANSI/OEM) */
	NULL,NULL,NULL,NULL,NULL,NULL,
	NULL, /* 87 - Current ANSI CP ANSI */
	"cp1252", /* 88 - Western European ANSI */
	"cp1252", /* 89 - Spanish ANSI */
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
	"cp852", /* 100 - Eastern European MS-DOS */
	"cp866", /* 101 - Russian MS-DOS */
	"cp865", /* 102 - Nordic MS-DOS */
	"cp861", /* 103 - Icelandic MS-DOS */
	"cp850", /* 104 - Kamenicky (Czech) MS-DOS 895 */
	"cp850", /* 105 - Mazovia (Polish) MS-DOS 620 */
	"cp737", /* 106 - Greek MS-DOS (437G) */
	"cp857", /* 107 - Turkish MS-DOS */
	"cp863", /* 108 - French-Canadian MS-DOS */
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
	"cp950", /* 120 - Taiwan Big 5 */
	"cp949", /* 121 - Hangul (Wansung) */
	"cp936", /* 122 - PRC GBK */
	"cp932", /* 123 - Japanese Shift-JIS */
	"cp874", /* 124 - Thai Windows/MS–DOS */
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
	"cp737", /* 134 - Greek OEM */
	"cp852", /* 135 - Slovenian OEM */
	"cp857", /* 136 - Turkish OEM */
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
	"macCyrillic", /* 150 - Russian Macintosh */
	"macCentEuro", /* 151 - Eastern European Macintosh */
	"macGreek", /* 152 - Greek Macintosh */
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
	"cp1250", /* 200 - Eastern European Windows */
	"cp1251", /* 201 - Russian Windows */
	"cp1254", /* 202 - Turkish Windows */
	"cp1253", /* 203 - Greek Windows */
	"cp1257", /* 204 - Baltic Windows */
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
	NULL,NULL,NULL
	};

static char *get_encoding (char *codepage) {
	int result = 0; 
    if (codepage && strncmp(codepage,"LDID/",5) == 0) {
        result = atoi(codepage + 5);
        if (result < 0 || result > 255)
        	result = 0;
    };
    return codepages[result];
};

static SHPDate *get_date (SHPDate *date, char *str) {
    if (3 != sscanf(str,"%4d%2d%2d",&date->year,&date->month,&date->day)) {
        date->year = 0;
        date->month = 0;
        date->day = 0;
    }
    return date;	
}

static char *empty = "";
static char *failure = "0";
static char *success = "1";
static int record_count = 0;
static char message[512];

int process_dbf_cmd (ClientData clientData, Tcl_Interp *interp, int objc,  Tcl_Obj * CONST objv[]) {
	int i,j,k;
	DBFHandle df;
	Tcl_Encoding enc;
	Tcl_Obj *obj;
	int fc,rc;

	if (!clientData) {
		Tcl_SetResult (interp,"clientData is null",TCL_STATIC);
		return (TCL_ERROR);
		}

	df = ((struct dbf_info *) clientData)->df;
	enc = ((struct dbf_info *) clientData)->enc;

	if (objc > 1) {
		const char *command = Tcl_GetString(objv[1]);

		/*--------------------------------------------------------------*\
		 | info returns record count and field count
		\*--------------------------------------------------------------*/

		if (strcmp (command,"info") == 0) {

			if (!df) {
				Tcl_SetResult (interp,"info: cannot find; no dbf has been read",TCL_STATIC);
				return (TCL_ERROR);
				}

			rc = DBFGetRecordCount (df);
			fc = DBFGetFieldCount  (df);

			Tcl_ResetResult (interp);
			if ((obj = Tcl_NewListObj (0,NULL)) == NULL) {
				Tcl_SetResult(interp,"info: failed to obtain a list object for results",TCL_STATIC);
				return (TCL_ERROR);
				}

			char number[16];

			sprintf (number,"%d",rc);
			Tcl_ListObjAppendElement (interp,obj,Tcl_NewStringObj (number,-1));

			sprintf (number,"%d",fc);
			Tcl_ListObjAppendElement (interp,obj,Tcl_NewStringObj (number,-1));

			Tcl_SetObjResult (interp,obj);
			return (TCL_OK);
			}

		/*--------------------------------------------------------------*\
		 | codepage returns record count and field count
		\*--------------------------------------------------------------*/

		if (strcmp (command,"codepage") == 0) {

			Tcl_SetObjResult (interp,Tcl_NewStringObj (DBFGetCodePage(df),-1));
			return (TCL_OK);
		}

		/*--------------------------------------------------------------*\
		 | add a field to the dbf
		\*--------------------------------------------------------------*/

		if (strcmp (command,"add") == 0) {

			if (!df) {
				Tcl_SetResult (interp,"add: cannot find this dbf; no dbf has been created",TCL_STATIC);
				return (TCL_ERROR);
				}

			if (objc > 2) {
				char *field_name = Tcl_GetString(objv[2]);
				char *s;

				/* Make sure that name is 10 characters or less and is composed only of letters, numbers, and underscore */

				int ok = 1;
				if (strlen (field_name) > 10) ok = 0;
				for (s=field_name; *s; s++)
					if (!(isalnum(*s) || *s == '_')) {
						ok = 0;
						break;
						}
				if (!ok) {
					Tcl_SetResult (interp,"add: field name must be 10 characters or less, and contain only letters, numbers, or underscore",TCL_STATIC);
					return (TCL_ERROR);
					}

				if (objc > 3) {
					DBFFieldType field_type = get_type (Tcl_GetString(objv[3]));

					if (field_type != FTInvalid) {
						if (objc > 4) {
							int field_width = 0;
							int field_prec  = 0;

							if (Tcl_GetIntFromObj(interp,objv[4],&field_width) == TCL_ERROR) {
								Tcl_SetResult (interp,"add: cannot interpret the width of the field",TCL_STATIC);
								return (TCL_ERROR);
								}
							if (field_width < 1 || field_width > 255) {
								Tcl_SetResult (interp,"add: field width must be greater than zero and less than 256",TCL_STATIC);
								return (TCL_ERROR);
								}

							if (field_type == FTDouble) {
								if (objc > 5) {
								/*	field_prec = (int) strtoul (Tcl_GetString(objv[5]),&t,0); */
									if (Tcl_GetIntFromObj(interp,objv[5],&field_prec) == TCL_ERROR) {
										Tcl_SetResult (interp,"add: cannot interpret the precision of the field",TCL_STATIC);
										return (TCL_ERROR);
										}
									if (field_prec > field_width) {
										Tcl_SetResult (interp,"add: field prec must not be greater than field width",TCL_STATIC);
										return (TCL_ERROR);
										}
									}
								}

							/* Try to add the field */

							j = DBFAddField (df,field_name,field_type,field_width,field_prec);

							if (j >= 0) {
								char number[16];
								sprintf (number,"%d",j);
								Tcl_SetResult (interp,number,TCL_STATIC);
								return (TCL_OK);
								}
							else {
								Tcl_SetResult (interp,"add: field could not be added.  Fields can be added only after creating the file and before adding any records.",TCL_STATIC);
								return (TCL_ERROR);
								}
							}
						else {
							Tcl_SetResult (interp,"add: width of field is required",TCL_STATIC);
							return (TCL_ERROR);
							}
						}
					else {
						Tcl_SetResult (interp,"add: type of field must be String, Integer, Logical, Date, or Double",TCL_STATIC);
						return (TCL_ERROR);
						}
					}
				else {
					Tcl_SetResult (interp,"add: type of field is required",TCL_STATIC);
					return (TCL_ERROR);
					}
				}
			else {
				Tcl_SetResult (interp,"add: the name of a field must be specified",TCL_STATIC);
				return (TCL_ERROR);
				}
			}

		/*--------------------------------------------------------------*\
		 | fields
		\*--------------------------------------------------------------*/

		if (strcmp (command,"fields") == 0) {
			struct field_info *info;

			if (!df) {
				Tcl_SetResult (interp,"fields: cannot find; no dbf has been read",TCL_STATIC);
				return (TCL_ERROR);
				}

			fc = DBFGetFieldCount (df);

			/* Allocate space for the field info */

			if (info = (struct field_info *) malloc (fc * sizeof (struct field_info))) {
				for (j=0; j < fc; j++) {
					info[j].type = DBFGetFieldInfo (df,j,info[j].name,&info[j].width, &info[j].precision);
					info[j].native = DBFGetNativeFieldType(df,j);
					}

				if (objc > 2) {
					char *field = Tcl_GetString(objv[2]);
					char number[16];
					char *t;

					if ((j = DBFGetFieldIndex (df,field)) == -1) {
						sprintf (message,"fields %s does not match a field name in this dbf file",field);
						Tcl_SetResult (interp,message,TCL_STATIC);
						return (TCL_ERROR);
						}

					Tcl_ResetResult (interp);
					obj = Tcl_NewListObj (0,NULL);
					t = info[j].name;
					Tcl_ListObjAppendElement (interp,obj,Tcl_NewStringObj (t,-1));
					t = type_of (info[j].type);
					Tcl_ListObjAppendElement (interp,obj,Tcl_NewStringObj (t,-1));
					t = &info[j].native;
					Tcl_ListObjAppendElement (interp,obj,Tcl_NewStringObj (t,1));
					sprintf (number,"%d",info[j].width);
					Tcl_ListObjAppendElement (interp,obj,Tcl_NewStringObj (number,-1));
					sprintf (number,"%d",info[j].precision);
					Tcl_ListObjAppendElement (interp,obj,Tcl_NewStringObj (number,-1));
					}
				else {
					Tcl_ResetResult (interp);
					obj = Tcl_NewListObj (0,NULL);
					for (j=0; j < fc; j++) {
						Tcl_Obj *sub;
						char number[16];
						char *t;
						sub = Tcl_NewListObj (0,NULL);
						t = info[j].name;
						Tcl_ListObjAppendElement (interp,sub,Tcl_NewStringObj (t,-1));
						t = type_of (info[j].type);
						Tcl_ListObjAppendElement (interp,sub,Tcl_NewStringObj (t,-1));
						t = &info[j].native;
						Tcl_ListObjAppendElement (interp,sub,Tcl_NewStringObj (t,1));
						sprintf (number,"%d",info[j].width);
						Tcl_ListObjAppendElement (interp,sub,Tcl_NewStringObj (number,-1));
						sprintf (number,"%d",info[j].precision);
						Tcl_ListObjAppendElement (interp,sub,Tcl_NewStringObj (number,-1));

						Tcl_ListObjAppendElement (interp,obj,sub);
						}
					}
				free (info);
				Tcl_SetObjResult (interp,obj);
				return (TCL_OK);
				}
			else {
				Tcl_SetResult (interp,"fields: cannot get field information",TCL_STATIC);
				return (TCL_ERROR);
				}
			}

		/*--------------------------------------------------------------*\
		 | values <field>
		\*--------------------------------------------------------------*/

		if (strcmp (command,"values") == 0)
			if (objc > 2) {
				char *field = Tcl_GetString(objv[2]);

				if (!df) {
					Tcl_SetResult (interp,"values: cannot find; no dbf has been read",TCL_STATIC);
					return (TCL_ERROR);
					}

				if ((j = DBFGetFieldIndex (df,field)) == -1) {
					sprintf (message,"values %s does not match a field name in this dbf file",field);
					Tcl_SetResult (interp,message,TCL_STATIC);
					return (TCL_ERROR);
					}

				rc = DBFGetRecordCount(df);
				obj = Tcl_NewListObj (0,NULL);
				for (i=0; i < rc; i++)
					if (DBFIsAttributeNULL (df,i,j))
						Tcl_ListObjAppendElement (interp,obj,Tcl_NewStringObj (empty,0));
					else {
						const char *t;
						Tcl_DString e;
						Tcl_DStringInit(&e);
						t = DBFReadStringAttribute (df,i,j);
						Tcl_ListObjAppendElement (interp,obj,Tcl_NewStringObj (Tcl_ExternalToUtfDString(enc, t, -1, &e),-1));
						Tcl_DStringFree(&e);
						}
				Tcl_SetObjResult (interp,obj);
				return (TCL_OK);
				}
			else {
				Tcl_SetResult (interp,"values expects the name of a field",TCL_STATIC);
				return (TCL_ERROR);
				}

		/*--------------------------------------------------------------*\
		 | record <number>
		\*--------------------------------------------------------------*/

		if (strcmp (command,"record") == 0)
			if (objc > 2) {
				if (!df) {
					Tcl_SetResult (interp,"record: cannot find; no dbf has been read",TCL_STATIC);
					return (TCL_ERROR);
					}

				fc = DBFGetFieldCount (df);
				rc = DBFGetRecordCount(df);

				if (Tcl_GetIntFromObj(interp,objv[2],&i) == TCL_ERROR) {
					Tcl_SetResult (interp,"record cannot interpret the number of the record",TCL_STATIC);
					return (TCL_ERROR);
					}

				if (i < 0 || i >= rc) {
					Tcl_SetResult (interp,"record number out of range",TCL_STATIC);
					return (TCL_ERROR);
					}

				obj = Tcl_NewListObj (0,NULL);
				for (j=0; j < fc; j++)
					if (DBFIsAttributeNULL (df,i,j))
						Tcl_ListObjAppendElement (interp,obj,Tcl_NewStringObj (empty,0));
					else {
						const char *t;
						Tcl_DString e;
						Tcl_DStringInit(&e);
						t = DBFReadStringAttribute (df,i,j);
						Tcl_ListObjAppendElement (interp,obj,Tcl_NewStringObj (Tcl_ExternalToUtfDString(enc, t, -1, &e),-1));
						Tcl_DStringFree(&e);
						}
				Tcl_SetObjResult (interp,obj);
				return (TCL_OK);
				}
			else {
				Tcl_SetResult (interp,"record expects the number of a record",TCL_STATIC);
				return (TCL_ERROR);
				}

		/*--------------------------------------------------------------*\
		 | insert <number> | end  <values>
		\*--------------------------------------------------------------*/

		if (strcmp (command,"insert") == 0)
			if (objc > 2) {
				char *rowid = Tcl_GetString(objv[2]);
				char *t;

				if (!df) {
					Tcl_SetResult (interp,"insert: cannot find dbf; no dbf has been created",TCL_STATIC);
					return (TCL_ERROR);
					}

				fc = DBFGetFieldCount (df);
				rc = DBFGetRecordCount(df);

				if (strcmp (rowid,"end") == 0) i = rc;
				else {
					if (Tcl_GetIntFromObj(interp,objv[2],&i) == TCL_ERROR) {
						Tcl_SetResult (interp,"insert: cannot interpret the number of the record",TCL_STATIC);
						return (TCL_ERROR);
						}
					}

				if (i < 0 || i > rc) {
					Tcl_SetResult (interp,"insert: record number out of range",TCL_STATIC);
					return (TCL_ERROR);
					}

				/* If the third argument is a list, get the values to be inserted from it */

				if (objv[3]->typePtr)
					if (objv[3]->typePtr->name)
						if (strcmp(objv[3]->typePtr->name,"list") == 0) {
							int value_objc;
							Tcl_Obj **value_objv;

							if (Tcl_ListObjGetElements (interp,objv[3],&value_objc,&value_objv) == TCL_ERROR) {
								Tcl_SetResult (interp,"linsert: expected a Tcl list as an argument",TCL_STATIC);
								return (TCL_ERROR);
								}

							k = 0;
							for (j=0; j < value_objc; j++) {
								DBFFieldType field_type;
								char field_name[12];
								int field_width;
								int integer_value;
								double double_value;
								SHPDate date_value;
								char *value = Tcl_GetString(value_objv[j]);

								if (strlen(value) == 0) {
									DBFWriteNULLAttribute (df,i,k);
									}
								else {
									field_type = DBFGetFieldInfo (df,k,field_name,&field_width,NULL);
									switch (field_type) {
										case FTString:
											{
											Tcl_DString e;
											Tcl_DStringInit(&e);
											if (!DBFWriteStringAttribute (df,i,k,Tcl_UtfToExternalDString(enc, value, -1, &e))) {
												fprintf (stderr,"Warning: value truncated when writing to field %s\n",field_name);
												fprintf (stderr,"         value is \"%s\"\n",value);
												}
										    Tcl_DStringFree(&e);
											}
											break;
										case FTInteger:
											if (Tcl_GetIntFromObj(interp,value_objv[j],&integer_value) == TCL_ERROR) {
												Tcl_SetResult (interp,"insert: cannot interpret integer value",TCL_STATIC);
												return (TCL_ERROR);
												}
											if (!DBFWriteIntegerAttribute (df,i,k,integer_value))
												fprintf (stderr,"Warning: failed to write number %d to field %s\n",integer_value,field_name);
											break;
										case FTDouble:
											if (Tcl_GetDoubleFromObj(interp,value_objv[j],&double_value) == TCL_ERROR) {
												Tcl_SetResult (interp,"insert: cannot interpret double value",TCL_STATIC);
												return (TCL_ERROR);
												}
											if (!DBFWriteDoubleAttribute (df,i,k,double_value))
												fprintf (stderr,"Warning: failed to write number %lf to field %s\n",double_value,field_name);
											break;
										case FTLogical:
											if (!DBFWriteLogicalAttribute (df,i,k,*value)) {
												fprintf (stderr,"Warning: logical value unrecognized for field %s\n",field_name);
												fprintf (stderr,"         value is \"%s\"\n",value);
												}
											break;
										case FTDate:
											if (!DBFWriteDateAttribute (df,i,k,get_date(&date_value,value))) {
												fprintf (stderr,"Warning: date value unrecognized for field %s\n",field_name);
												fprintf (stderr,"         value is \"%s\"\n",value);
												}
											break;
										case FTInvalid:
										default:
											fprintf (stderr,"Warning: Field %d is an unwritable field of type %s\n",k,type_of(field_type));
											break;
										}
									}
								k++;
								}
							sprintf (message,"%d",i);
							Tcl_SetResult (interp,message,TCL_STATIC);
							return (TCL_OK);
							}

				k = 0;
				for (j=3; j < objc; j++) {
					char *value = Tcl_GetString(objv[j]);
					DBFFieldType field_type;
					char field_name[12];
					int field_width;
					int integer_value;
					double double_value;
					SHPDate date_value;

					if (strlen(value) == 0) {
						DBFWriteNULLAttribute (df,i,k);
						}
					else {
						field_type = DBFGetFieldInfo (df,k,field_name,&field_width,NULL);
						switch (field_type) {
							case FTString:
								{
								Tcl_DString e;
								Tcl_DStringInit(&e);
								if (!DBFWriteStringAttribute (df,i,k,Tcl_UtfToExternalDString(enc, value, -1, &e))) {
									fprintf (stderr,"Warning: value truncated when writing to field %s\n",field_name);
									}
								Tcl_DStringFree(&e);
								}
								break;
							case FTInteger:
							case FTDouble:
								double_value = (double) strtod (value,&t);
								if (t == value) {
									Tcl_SetResult (interp,"insert: cannot interpret double value",TCL_STATIC);
									return (TCL_ERROR);
									}
								if (!DBFWriteDoubleAttribute (df,i,k,double_value))
									fprintf (stderr,"Warning: failed to write number %lf to field %s\n",double_value,field_name);
								break;
							case FTLogical:
								if (!DBFWriteLogicalAttribute (df,i,k,*value)) {
									fprintf (stderr,"Warning: logical value unrecognized for field %s\n",field_name);
									fprintf (stderr,"         value is \"%s\"\n",value);
									}
								break;
							case FTDate:
								if (!DBFWriteDateAttribute (df,i,k,get_date(&date_value,value))) {
									fprintf (stderr,"Warning: date value unrecognized for field %s\n",field_name);
									fprintf (stderr,"         value is \"%s\"\n",value);
									}
								break;
							case FTInvalid:
							default:
								fprintf (stderr,"Warning: Field %d is an unwritable field of type %s\n",k,type_of(field_type));
								break;
							}
						}
					k++;
					}
				sprintf (message,"%d",i);
				Tcl_SetResult (interp,message,TCL_STATIC);
				return (TCL_OK);
				}
			else {
				Tcl_SetResult (interp,"insert expects the number of a record or 'end'",TCL_STATIC);
				return (TCL_ERROR);
				}

		/*--------------------------------------------------------------*\
		 | update <record> <field> <value>
		\*--------------------------------------------------------------*/

		if (strcmp (command,"update") == 0)
			if (objc > 2) {
				char *rowid = Tcl_GetString(objv[2]);
				char *t;

				if (!df) {
					Tcl_SetResult (interp,"update: cannot find dbf; no dbf has been created",TCL_STATIC);
					return (TCL_ERROR);
					}

				fc = DBFGetFieldCount (df);
				rc = DBFGetRecordCount(df);

				i = (int) strtoul (rowid,&t,0);
				if (t == rowid) {
					Tcl_SetResult (interp,"update: cannot interpret the number of the record",TCL_STATIC);
					return (TCL_ERROR);
					}

				if (i < 0 || i >= rc) {
					Tcl_SetResult (interp,"update: record number out of range",TCL_STATIC);
					return (TCL_ERROR);
					}

				if (objc > 3) {
					char *field_name = Tcl_GetString(objv[3]);

					k = DBFGetFieldIndex (df,field_name);
					if (k != -1) {
						if (objc > 4) {
							char *value = Tcl_GetString(objv[4]);
							DBFFieldType field_type;
							double double_value;
						    SHPDate date_value;

							field_type = DBFGetFieldInfo (df,k,field_name,NULL,NULL);
							switch (field_type) {
								case FTString:
									{
									Tcl_DString e;
									Tcl_DStringInit(&e);
									if (!DBFWriteStringAttribute (df,i,k,Tcl_UtfToExternalDString(enc, value, -1, &e))) {
										fprintf (stderr,"Warning: value truncated when writing to field %s\n",field_name);
										}
									Tcl_DStringFree(&e);
									}
									break;
								case FTInteger:
								case FTDouble:
									double_value = (double) strtod (value,&t);
									if (t == value) {
										Tcl_SetResult (interp,"insert: cannot interpret double value",TCL_STATIC);
										return (TCL_ERROR);
										}
									if (!DBFWriteDoubleAttribute (df,i,k,double_value))
										fprintf (stderr,"Warning: failed to write number %lf to field %s\n",double_value,field_name);
									break;
								case FTLogical:
									if (!DBFWriteLogicalAttribute (df,i,k,*value)) {
										fprintf (stderr,"Warning: logical value unrecognized for field %s\n",field_name);
										fprintf (stderr,"         value is \"%s\"\n",value);
										}
									break;
								case FTDate:
									if (!DBFWriteDateAttribute (df,i,k,get_date(&date_value,value))) {
										fprintf (stderr,"Warning: date value unrecognized for field %s\n",field_name);
										fprintf (stderr,"         value is \"%s\"\n",value);
										}
									break;
								case FTInvalid:
								default:
									fprintf (stderr,"Warning: Field %d is an unwritable field of type %s\n",k,type_of(field_type));
									break;
								}
							Tcl_SetResult (interp,success,TCL_STATIC);
							return (TCL_OK);
							}
						else {
							Tcl_SetResult (interp,"update: specify value for field",TCL_STATIC);
							return (TCL_ERROR);
							}
						}
					else {
						Tcl_SetResult (interp,"update: field not found in the dbf file",TCL_STATIC);
						return (TCL_ERROR);
						}
					}
				else {
					Tcl_SetResult (interp,"update: specify field to update",TCL_STATIC);
					return (TCL_ERROR);
					}
				}
			else {
				Tcl_SetResult (interp,"update expects the number of an existing record",TCL_STATIC);
				return (TCL_ERROR);
				}

		/*--------------------------------------------------------------*\
		 | deleted <record> [<value>]
		\*--------------------------------------------------------------*/

		if (strcmp (command,"deleted") == 0)
			if (objc > 2) {
				char *rowid = Tcl_GetString(objv[2]);
				char *t;

				if (!df) {
					Tcl_SetResult (interp,"update: cannot find dbf; no dbf has been created",TCL_STATIC);
					return (TCL_ERROR);
					}

				fc = DBFGetFieldCount (df);
				rc = DBFGetRecordCount(df);

				i = (int) strtoul (rowid,&t,0);
				if (t == rowid) {
					Tcl_SetResult (interp,"update: cannot interpret the number of the record",TCL_STATIC);
					return (TCL_ERROR);
					}

				if (i < 0 || i >= rc) {
					Tcl_SetResult (interp,"update: record number out of range",TCL_STATIC);
					return (TCL_ERROR);
					}

				if (objc > 3) {
					int b;
					if (Tcl_GetBooleanFromObj(interp,objv[3],&b) != TCL_OK) {
						fprintf (stderr,"Warning: invalid boolean value\n");
						return (TCL_ERROR);
						}
					if (!DBFMarkRecordDeleted(df,i,b)) {
						fprintf (stderr,"Warning: failed to change deleted mark\n");
						return (TCL_ERROR);
						}
				}
				Tcl_SetObjResult(interp,Tcl_NewIntObj(DBFIsRecordDeleted(df,i)));
				return (TCL_OK);
				}
			else {
				Tcl_SetResult (interp,"deleted expects the number of an existing record",TCL_STATIC);
				return (TCL_ERROR);
				}
#ifdef TEST

		/*--------------------------------------------------------------*\
		 | test (used for understanding command arguments)
		\*--------------------------------------------------------------*/

		if (strcmp (command,"test") == 0) {
			if (objc > 2) {
				int value_objc = 0;
				Tcl_Obj **value_objv = NULL;

				printf ("objc = %d\n",objc);
				for (i=0; i < objc; i++)
					if (objv[i]) {
						printf ("objv[%d] at 0x%X",i,objv[i]);
						if (objv[i]->typePtr)
							if (objv[i]->typePtr->name)
								printf (" is a %s",objv[i]->typePtr->name);
							else
								printf (" has no name");
						else
							printf (" has no typePtr");
						printf (" and value \"%s\"",Tcl_GetString(objv[i]));
						printf ("\n");
						}
					else
						printf ("objv[%d] is NULL\n",i);

				if (objc == 3) {
					if (Tcl_ListObjGetElements (interp,objv[2],&value_objc,&value_objv) == TCL_ERROR) {
						Tcl_SetResult (interp,"linsert: expected a Tcl list as an argument",TCL_STATIC);
						return (TCL_ERROR);
						}
					printf ("value_objc = %d\n",value_objc);
					printf ("value_objv = 0x%X\n",value_objv);
					if (value_objv)
						for (i=0; i < value_objc; i++)
							if (value_objv[i]) {
								printf ("value_objv[%d] at 0x%X",i,value_objv[i]);
								if (value_objv[i]->typePtr)
									if (value_objv[i]->typePtr->name)
										printf (" is a %s",value_objv[i]->typePtr->name);
									else
										printf (" has no name");
								else
									printf (" has no typePtr");
								printf (" and value \"%s\"",Tcl_GetString(value_objv[i]));
								printf ("\n");
								}
					}
				}
			return (TCL_OK);
			}
#endif
		/*--------------------------------------------------------------*\
		 | forget == close
		\*--------------------------------------------------------------*/

		if (strcmp (command,"forget") == 0 || strcmp (command,"close") == 0) {
			if (df) {
				Tcl_FreeEncoding(enc);
				DBFClose (df);
				free(clientData);
				Tcl_DeleteCommand (interp,Tcl_GetString(objv[0]));
				Tcl_SetResult (interp,success,TCL_STATIC);
				}
			else
				Tcl_SetResult (interp,failure,TCL_STATIC);
			return (TCL_OK);
			}

		}
	return (TCL_OK);
	}

int dbf_cmd (ClientData clientData, Tcl_Interp *interp, int objc,  Tcl_Obj * CONST objv[]) {
	char *variable_name;
	char *input_file;
	char *output_file;
	char id [64];
	char *mode;
	char *text_buffer = NULL;
	DBFHandle df;

	Tcl_ResetResult (interp);

	if (objc > 1) {
		variable_name = Tcl_GetString(objv[1]);
		if (objc > 1) {

			/*----------------------------------------------------------*\
			 | -open input_file
			\*----------------------------------------------------------*/

			if (strcmp (Tcl_GetString(objv[2]),"-open") == 0) {
				if (objc > 2) {
					Tcl_DString s;
					Tcl_DString e;
					Tcl_DStringInit(&s);
					Tcl_DStringInit(&e);

      				input_file = Tcl_UtfToExternalDString(NULL, Tcl_TranslateFileName(interp, Tcl_GetString(objv[3]), &s), -1, &e);

					mode = "rb+";
					if (objc > 4)
						if (strcmp (Tcl_GetString(objv[4]),"-readonly") == 0) mode = "rb";

					/*--------------------------------------------------*\
					 | Open the input file creating a new command.		|
					\*--------------------------------------------------*/

					if (df = DBFOpen (input_file,mode)) {
						struct dbf_info * di = malloc (sizeof (struct dbf_info));
						di->df = df;
						di->enc = Tcl_GetEncoding(NULL,get_encoding(df->pszCodePage));
						sprintf (id,"dbf.%04X",record_count++);
						Tcl_SetVar (interp,variable_name,id,0);
						Tcl_CreateObjCommand (interp,id,(Tcl_ObjCmdProc *) process_dbf_cmd,(ClientData)di, (Tcl_CmdDeleteProc *)NULL);
						Tcl_SetResult (interp,success,TCL_STATIC);
						Tcl_DStringFree(&e);
						Tcl_DStringFree(&s);
						return (TCL_OK);
						}
					else {
						sprintf (message,"Error: could not open input file %s",input_file);
						Tcl_SetResult (interp,message,TCL_STATIC);
						Tcl_DStringFree(&e);
						Tcl_DStringFree(&s);
						return (TCL_ERROR);
						}
					}
				else {
					sprintf (message,"Error: no input file name given");
					Tcl_SetResult (interp,message,TCL_STATIC);
					return (TCL_ERROR);
					}
				}

			/*----------------------------------------------------------*\
			 | -create output_file
			\*----------------------------------------------------------*/

			if (strcmp (Tcl_GetString(objv[2]),"-create") == 0) {
				if (objc > 2) {
					char *codepage = "LDID/87"; /* 87 - ANSI, 38 - 866, 201 - 1251 */
					Tcl_DString s;
					Tcl_DString e;
					Tcl_DStringInit(&s);
					Tcl_DStringInit(&e);

      				output_file = Tcl_UtfToExternalDString(NULL, Tcl_TranslateFileName(interp, Tcl_GetString(objv[3]), &s), -1, &e);

					if (objc > 5 && strcmp(Tcl_GetString(objv[4]),"-codepage") == 0)
						codepage = Tcl_GetString(objv[5]);

					/*--------------------------------------------------*\
					 | Open the input file creating a new command.		|
					\*--------------------------------------------------*/

					if (output_file)
						if (df = DBFCreateEx(output_file, codepage)) {
							struct dbf_info * di = malloc (sizeof (struct dbf_info));
							di->df = df;
							di->enc = Tcl_GetEncoding(NULL,get_encoding(df->pszCodePage));
							sprintf (id,"dbf.%04X",record_count++);
							Tcl_SetVar (interp,variable_name,id,0);
							Tcl_CreateObjCommand (interp,id,(Tcl_ObjCmdProc *) process_dbf_cmd,(ClientData)di, (Tcl_CmdDeleteProc *)NULL);
							Tcl_SetResult (interp,success,TCL_STATIC);
							}
						else
							Tcl_SetResult (interp,failure,TCL_STATIC);
					else
						Tcl_SetResult (interp,failure,TCL_STATIC);

					Tcl_DStringFree(&e);
					Tcl_DStringFree(&s);
					return (TCL_OK);
					}
				else {
					sprintf (message,"Error: no input file name given");
					Tcl_SetResult (interp,message,TCL_STATIC);
					return (TCL_ERROR);
					}
				}
			}
		else {
			sprintf (message,"Error: got variable name %s, no -open or -create",Tcl_GetString(objv[1]));
			Tcl_SetResult (interp,message,TCL_STATIC);
			return (TCL_ERROR);
			}
		}
	else {
		Tcl_SetResult (interp,"dbf expects variable name",TCL_STATIC);
		return (TCL_ERROR);
		}
	return (TCL_OK);
	}


int Dbf_Init (Tcl_Interp *interp) {
	if (Tcl_InitStubs (interp,"8.1",0) == NULL) return (TCL_ERROR);
	if (Tcl_PkgRequire (interp,"Tcl","8.0",0) == NULL) return (TCL_ERROR);
	if (Tcl_PkgProvide (interp,PACKAGE_NAME,PACKAGE_VERSION) != TCL_OK) return (TCL_ERROR);

	Tcl_CreateObjCommand (interp,"dbf",(Tcl_ObjCmdProc *) dbf_cmd,(ClientData) NULL,(Tcl_CmdDeleteProc *) NULL);
	return (TCL_OK);
	}

/*----------------------------------------------------------------------*\
\*----------------------------------------------------------------------*/
