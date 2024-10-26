/*----------------------------------------------------------------------*\
 | C routines that read dBase files using Frank Warmerdam's shapelib	|
 | and make the results available to Tcl procedures.					|
 |	http://pobox.com/~warmerdam											|
 |																		|
 | What do I want to do with dbf files in Tcl?							|
 |																		|
 | dbf d -open $input_file [-readonly]									|
 |		opens dbase file, returns a handle.								|
 | dbf d -create $input_file											|
 |		creates dbase file, returns a handle							|
 |																		|
 | $d info																|
 |		returns {record_count field_count}								|
 |																		|
 | $d add label type width [prec]										|
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

static char *type_of (DBFFieldType t) {
	if (t == FTString ) return ("String" );
	if (t == FTInteger) return ("Integer");
	if (t == FTDouble ) return ("Double" );
	if (t == FTLogical) return ("Logical");
	if (t == FTInvalid) return ("Invalid");
	return ("Unknown");
	}

static DBFFieldType get_type (char *name) {
	DBFFieldType result = FTInvalid;
	if (name && *name) {
		if (strcmp (name,"String" ) == 0) result = FTString;
		if (strcmp (name,"Integer") == 0) result = FTInteger;
		if (strcmp (name,"Double" ) == 0) result = FTDouble;
		if (strcmp (name,"Logical") == 0) result = FTLogical;
		}
	return (result);
	}

static char *empty = "";
static char *failure = "0";
static char *success = "1";
static int record_count = 0;
static char message[512];

int process_dbf_cmd (ClientData clientData, Tcl_Interp *interp, int objc,  Tcl_Obj * CONST objv[]) {
	int i,j,k;
	DBFHandle df = (DBFHandle) clientData;
	Tcl_Obj *obj;
	int fc,rc;

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
						Tcl_SetResult (interp,"add: type of field must be String, Integer, Logical, or Double",TCL_STATIC);
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
						t = DBFReadStringAttribute (df,i,j);
						Tcl_ListObjAppendElement (interp,obj,Tcl_NewStringObj (t,strlen(t)));
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
						t = DBFReadStringAttribute (df,i,j);
						Tcl_ListObjAppendElement (interp,obj,Tcl_NewStringObj (t,strlen(t)));
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
								char *value = Tcl_GetString(value_objv[j]);

								if (strlen(value) == 0) {
									DBFWriteNULLAttribute (df,i,k);
									}
								else {
									field_type = DBFGetFieldInfo (df,k,field_name,&field_width,NULL);
									switch (field_type) {
										case FTString:
											if (!DBFWriteStringAttribute (df,i,k,value)) {
												fprintf (stderr,"Warning: value truncated when writing to field %s\n",field_name);
												fprintf (stderr,"         value is \"%s\"\n",value);
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

					if (strlen(value) == 0) {
						DBFWriteNULLAttribute (df,i,k);
						}
					else {
						field_type = DBFGetFieldInfo (df,k,field_name,&field_width,NULL);
						switch (field_type) {
							case FTString:
								if (!DBFWriteStringAttribute (df,i,k,value)) {
									fprintf (stderr,"Warning: value truncated when writing to field %s\n",field_name);
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

							field_type = DBFGetFieldInfo (df,k,field_name,NULL,NULL);
							switch (field_type) {
								case FTString:
									if (!DBFWriteStringAttribute (df,i,k,value)) {
										fprintf (stderr,"Warning: value truncated when writing to field %s\n",field_name);
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
		 | test (used for understanding command arguments)
		\*--------------------------------------------------------------*/
#ifdef TEST
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
				DBFClose (df);
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
					input_file = Tcl_GetString(objv[3]);

					mode = "rb+";
					if (objc > 4)
						if (strcmp (Tcl_GetString(objv[4]),"-readonly") == 0) mode = "rb";

					/*--------------------------------------------------*\
					 | Open the input file creating a new command.		|
					\*--------------------------------------------------*/

					if (df = DBFOpen (input_file,mode)) {
						sprintf (id,"dbf.%04X",record_count++);
						Tcl_SetVar (interp,variable_name,id,0);
						Tcl_CreateObjCommand (interp,id,(Tcl_ObjCmdProc *) process_dbf_cmd,(ClientData)df, (Tcl_CmdDeleteProc *)NULL);
						Tcl_SetResult (interp,success,TCL_STATIC);
						return (TCL_OK);
						}
					else {
						sprintf (message,"Error: could not open input file %s",input_file);
						Tcl_SetResult (interp,message,TCL_STATIC);
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
					output_file = Tcl_GetString(objv[3]);

					/*--------------------------------------------------*\
					 | Open the input file creating a new command.		|
					\*--------------------------------------------------*/

					if (output_file)
						if (df = DBFCreate (output_file)) {
							sprintf (id,"dbf.%04X",record_count++);
							Tcl_SetVar (interp,variable_name,id,0);
							Tcl_CreateObjCommand (interp,id,(Tcl_ObjCmdProc *) process_dbf_cmd,(ClientData)df, (Tcl_CmdDeleteProc *)NULL);
							Tcl_SetResult (interp,success,TCL_STATIC);
							}
						else
							Tcl_SetResult (interp,failure,TCL_STATIC);
					else
						Tcl_SetResult (interp,failure,TCL_STATIC);

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
	if (Tcl_PkgProvide (interp,"dbf","1.3") != TCL_OK) return (TCL_ERROR);

	Tcl_CreateObjCommand (interp,"dbf",(Tcl_ObjCmdProc *) dbf_cmd,(ClientData) NULL,(Tcl_CmdDeleteProc *) NULL);
	return (TCL_OK);
	}

/*----------------------------------------------------------------------*\
\*----------------------------------------------------------------------*/
