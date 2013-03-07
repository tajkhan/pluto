/*
 * sica_func.c
 *
 *  Created on: 06.03.2013
 *      Author: dfeld
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "hwanalysis.h"

#include "pluto.h"

#include "sica_accesses.h"

#include "sica_func.h"

int sica_get_bytes_of_type(char *data_type)    {

	int new_bytes=0;

	if(!strcmp(data_type,"int"))    {
		new_bytes=4;
	} else if(!strcmp(data_type,"float"))    {
		new_bytes=4;
	} else if(!strcmp(data_type,"double"))    {
		new_bytes=8;
	} else {
		printf("[SICA] WARNING: The datatype of an array was not recognized and therefore set to a DEFAULT VALUE!\n");
		new_bytes=SICA_DEFAULT_DATA_BYTES;
	}

	return new_bytes;
}


void sica_malloc_and_init_sicadata(Band **bands, int nbands)    {
int i, s;
    for (i=0; i<nbands; i++) {
        bands[i]->sicadata=(SICAData*)malloc(sizeof(SICAData));
        bands[i]->sicadata->isvec=-1;
        bands[i]->sicadata->vecloop=-1;
        bands[i]->sicadata->vecrow=-1;
        bands[i]->sicadata->sical1size=-1;
        bands[i]->sicadata->sical2size=-1;
        bands[i]->sicadata->upperboundoffset=(int*)malloc((bands[i]->loop->nstmts)*sizeof(int));
        for(s=0;s<bands[i]->loop->nstmts;s++)    {
        	bands[i]->sicadata->upperboundoffset[s]=-1;
        }
        bands[i]->sicadata->nb_arrays=0;
        bands[i]->sicadata->transwidth=0;
        bands[i]->sicadata->vec_accesses=0;
        bands[i]->sicadata->innermost_vec_accesses=0;
        bands[i]->sicadata->bytes_per_vecit=0;
        bands[i]->sicadata->largest_data_type=0;
    }
}





void sica_get_band_specific_tile_sizes(Band* act_band)    {

	int a,s,t,r,w,x,y,i;
		///////////////////////////////////////////////
	    //TEST THE NEW SICA

		/* [SICA] get an array<->id relation
		 *
		 * act_band->loop->stmts[s]->reads[r]->sym_id is not set in PluTo
		 * array with nb_arrays elements storing the name of the array
		 */

        /* [SICA] get number of all reads and writes as an upper bound of the number of arrays */
		int max_nb_arrays=0;
	    for(s=0; s<act_band->loop->nstmts;s++)
	    {
	    	max_nb_arrays+=act_band->loop->stmts[s]->nreads; /* number of total reads in this statement */
	    	max_nb_arrays+=act_band->loop->stmts[s]->nwrites; /* number of total writes in this statement */
	    }
	    IF_DEBUG2(printf("[SICA] max_nb_arrays=%i\n",max_nb_arrays););

		//[SICA] malloc the id2arrayname
		act_band->sicadata->id2arrayname=(char**)malloc(max_nb_arrays*sizeof(char*));
		for(a=0; a < max_nb_arrays; a++)    {
			act_band->sicadata->id2arrayname[a]=(char*)malloc(SICA_STRING_SIZE*sizeof(char));
        }

		//[SICA] init the id2arrayname
		for(a=0; a < max_nb_arrays; a++)    {
			for(t=0; t < SICA_STRING_SIZE; t++)    {
				act_band->sicadata->id2arrayname[a][t]='\0';
			}
        }

		/* [SICA] get the different arrays */
	    for(s=0; s<act_band->loop->nstmts;s++)    {
	    	/* [SICA] reads */
		    for(r=0;r<act_band->loop->stmts[s]->nreads;r++)    {
		    	/* [SICA] go through the array */
				int arraynameisnew=1; //store whether the prospected array name is already available or not
		    	a=0;
				while(strcmp(act_band->sicadata->id2arrayname[a],"\0"))    { //while there is a non-empty string in this array position
					//if there is one, check if it is equal to the one analysed at the moment
					if(!strcmp(act_band->sicadata->id2arrayname[a], act_band->loop->stmts[s]->reads[r]->name))    { //so they are identical
						arraynameisnew=0;
						break;
					}

					a++;
				}
				if(arraynameisnew) {
					//printf("[SICA] Copying the new name '%s' to the array to position %i\n", act_band->loop->stmts[s]->reads[r]->name, a);
					act_band->sicadata->id2arrayname[a]=strcpy(act_band->sicadata->id2arrayname[a], act_band->loop->stmts[s]->reads[r]->name);
					act_band->sicadata->nb_arrays++;
					//printf("[SICA] COPY SUCCEEDED\n");
				}
		    }

		    /* [SICA] writes */
		    for(w=0;w<act_band->loop->stmts[s]->nwrites;w++)
		    {
		    	/* [SICA] go through the array */
				int arraynameisnew=1; //store whether the prospected array name is already available or not
		    	a=0;
				while(strcmp(act_band->sicadata->id2arrayname[a],"\0"))    { //while there is a non-empty string in this array position
					//if there is one, check if it is equal to the one analysed at the moment
					if(!strcmp(act_band->sicadata->id2arrayname[a], act_band->loop->stmts[s]->writes[w]->name))    { //so they are identical
						arraynameisnew=0;
						break;
					}

					a++;
				}
				if(arraynameisnew) {
					//printf("[SICA] Copying the new name '%s' to the array to position %i\n", act_band->loop->stmts[s]->reads[r]->name, a);
					act_band->sicadata->id2arrayname[a]=strcpy(act_band->sicadata->id2arrayname[a], act_band->loop->stmts[s]->writes[w]->name);
					act_band->sicadata->nb_arrays++;
					//printf("[SICA] COPY SUCCEEDED\n");
				}
		    }
	    }

	    // [SICA] print the id2arrayname
	    IF_DEBUG(printf("[SICA] Detected %i different arrays\n", act_band->sicadata->nb_arrays););
		for(a=0; a < act_band->sicadata->nb_arrays; a++)    {
			IF_DEBUG(printf("[SICA] Array-ID: %i, name: %s\n", a, act_band->sicadata->id2arrayname[a]););
        }

		// [SICA] setup the empty array of sica_accesses structures
		SICAAccess** sica_accesses_on_array;
		sica_accesses_on_array=(SICAAccess**)malloc(act_band->sicadata->nb_arrays*sizeof(SICAAccess*));
		for(a=0; a<act_band->sicadata->nb_arrays; a++)    {
			sica_accesses_on_array[a]=sica_accesses_malloc();
		}


		IF_DEBUG(printf("[SICA] bands[%i], width=%i\n", i, act_band->width););
		IF_DEBUG(printf("[SICA] bands[%i], nstmts=%i\n", i, act_band->loop->nstmts););

	    for(s=0; s<act_band->loop->nstmts;s++)
	    {
	    	//// [SICA] print the transformation matrix
	    	//printf("T(S%i):\n",act_band->loop->stmts[s]->id);
			//pluto_matrix_print(stdout, act_band->loop->stmts[s]->trans);

	    	// [SICA] Print the extracted transformation matrix
	    	IF_DEBUG(printf("[SICA] Transformation matrix:\n"););
	    	IF_DEBUG(sica_print_matrix_with_coloffset(act_band->sicadata->trans, act_band->sicadata->transwidth, act_band->sicadata->transwidth, 0););

	    	// [SICA] Print the inverted transformation matrix
	    	IF_DEBUG(printf("[SICA] Inverted Transformation matrix:\n"););
	    	IF_DEBUG(sica_print_matrix_with_coloffset(act_band->sicadata->trans_inverted, act_band->sicadata->transwidth, act_band->sicadata->transwidth, 0););



	    	//->START READ ANALYSIS
	    	IF_DEBUG2(printf("[SICA] Analyse the READ accesses for vectorization\n"););
	    	IF_DEBUG2(printf("[SICA] stmt=%i: nreads=%i\n", s, act_band->loop->stmts[s]->nreads););
		    for(r=0;r<act_band->loop->stmts[s]->nreads;r++)
		    {
		    	IF_DEBUG2(printf("[SICA] read=%i, id: %i, name=%s, type=%s\n", r, sica_get_array_id(act_band, act_band->loop->stmts[s]->reads[r]->name), act_band->loop->stmts[s]->reads[r]->name, act_band->loop->stmts[s]->reads[r]->symbol->data_type););
		    	IF_DEBUG2(pluto_matrix_print(stdout, act_band->loop->stmts[s]->reads[r]->mat););

		    	//get the access matrix offset caused by tiling dimensions
		    	int access_offset=act_band->sicadata->transwidth;
		    	if(options->l2tile)    {
		    		access_offset=2*act_band->sicadata->transwidth;
		    	}

		    	//get the access matrix
		    	IF_DEBUG2(printf("[SICA] READ-ACCESS-MATRIX:\n"););
		    	IF_DEBUG2(sica_print_matrix_with_coloffset(act_band->loop->stmts[s]->reads[r]->mat->val, act_band->loop->stmts[s]->reads[r]->mat->alloc_nrows, act_band->sicadata->transwidth, access_offset););

		    	//go through all dimensions of this access and transform them
		    	int** orig_access_mat;
		    	int** trans_access_mat;

		    	//malloc the transformed mat to fill it
	    		orig_access_mat=(int**)malloc(act_band->loop->stmts[s]->reads[r]->mat->alloc_nrows*sizeof(int*));
	    		trans_access_mat=(int**)malloc(act_band->loop->stmts[s]->reads[r]->mat->alloc_nrows*sizeof(int*));

		    	for(y=0; y<act_band->loop->stmts[s]->reads[r]->mat->alloc_nrows; y++)    {
		    		orig_access_mat[y]=(int*)malloc(act_band->loop->stmts[s]->reads[r]->mat->alloc_ncols*sizeof(int));
		    		trans_access_mat[y]=(int*)malloc(act_band->loop->stmts[s]->reads[r]->mat->alloc_ncols*sizeof(int));
		    	}

		    	int orig_access_iterators[act_band->sicadata->transwidth];
		    	int trans_access_iterators[act_band->sicadata->transwidth];

		    	//fill the two access and row arrays
		    	for(y=0;y<act_band->loop->stmts[s]->reads[r]->mat->alloc_nrows;y++)    {

			    	for(x=0;x<act_band->loop->stmts[s]->reads[r]->mat->alloc_ncols;x++)    {
			    		orig_access_mat[y][x] = act_band->loop->stmts[s]->reads[r]->mat->val[y][x];
			    		trans_access_mat[y][x] = act_band->loop->stmts[s]->reads[r]->mat->val[y][x]; //after transformation overwrite the trans part
			    	}

			    	for(x=0;x<act_band->sicadata->transwidth;x++)    {
			    		orig_access_iterators[x] = act_band->loop->stmts[s]->reads[r]->mat->val[y][access_offset+x];
			    		trans_access_iterators[x] = 0;
				    	}

		    		sica_vec_times_matrix(trans_access_iterators,orig_access_iterators,act_band->sicadata->trans_inverted,act_band->sicadata->transwidth,act_band->sicadata->transwidth);

			    	//print the two access arrays
		    		//printf("ORIG-ACCESS:\t");
		    		//for(y=0;y<act_band->sicadata->transwidth;y++)    {
				    //		printf("%i ", orig_access[y]);
				    //	}
		    		//printf("\n");
                    //
		    		//printf("TRANS-ACCESS:\t");
		    		//for(y=0;y<act_band->sicadata->transwidth;y++)    {
				    //		printf("%i ", trans_access[y]);
				    //	}
		    		///printf("\n");

			    	//overwrite the transformed parts in the trans_row
			    	for(x=0;x<act_band->sicadata->transwidth;x++)    {
			    		trans_access_mat[y][access_offset+x] = trans_access_iterators[x];
			    	}
		    	}

		    	int act_array_id=sica_get_array_id(act_band, act_band->loop->stmts[s]->reads[r]->name);
		    	IF_DEBUG2(printf("[SICA] Looking up the following array name: '%s' with id: '%i' and transformed access matrix:\n", act_band->loop->stmts[s]->reads[r]->name, act_array_id););
		    	//This is now the transformed access
		    	IF_DEBUG2(sica_print_matrix_with_coloffset(trans_access_mat, act_band->loop->stmts[s]->reads[r]->mat->alloc_nrows, act_band->loop->stmts[s]->reads[r]->mat->alloc_ncols, 0););

		    	//Now we have all necessary data concerning this access, check if we have to count it for vectorized accesses!

		    	//check if the access is a just a scalar type, not a real array access (e.g. alpha or a[const][const])
		    	int entry_sum=sica_get_entry_sum(trans_access_mat, act_band->loop->stmts[s]->reads[r]->mat->alloc_nrows, act_band->loop->stmts[s]->reads[r]->mat->alloc_ncols);

		    	//IF it is a real array access...
		    	if(entry_sum)    {
		    		IF_DEBUG(printf("[SICA] The Access on Array '%s' is an array access!\n", act_band->loop->stmts[s]->reads[r]->name););
			    	for(y=0;y<act_band->loop->stmts[s]->reads[r]->mat->alloc_nrows;y++)    {

			    		//...CHECK IF there is one or more dimensions that is accessed by the vectorized loop and...
						if(act_band->sicadata->isvec&&trans_access_mat[y][access_offset+act_band->sicadata->vecrow])    {
							IF_DEBUG(printf("[SICA] VECTORIZATION: Array '%s' accesses dimension '%i' by a vectorized loop!\n", act_band->loop->stmts[s]->reads[r]->name, y););

							int sica_access_is_new=1;

							//...CHECK IF the access (matrix) is already recognized for this array (e.g. C[i][j]=C[i][j]+...). ...
							//printf("[SICA] ACT-POINTER: %p\n",sica_accesses_on_array[act_array_id]);
							SICAAccess* act_access_temp=sica_accesses_on_array[act_array_id];

							while(act_access_temp->next)    {
								//CHECK IF THE act_access_temp->mat is equal to the actual one
								int check_comparison=ACCESS_IS_NOT_IDENTICAL;
								check_comparison=sica_compare_access_matrices(trans_access_mat, act_access_temp->access_mat, act_band->loop->stmts[s]->reads[r]->mat->alloc_nrows, act_band->loop->stmts[s]->reads[r]->mat->alloc_ncols);

								//IF THEY ARE EQUAL
								if(check_comparison==ACCESS_IS_IDENTICAL)    {
									IF_DEBUG(printf("[SICA] THIS READ ACCESS ON '%s' IS ALREADY RECOGNIZED AND THEREFORE NOT ADDED!\n", act_band->loop->stmts[s]->reads[r]->name););
									sica_access_is_new=0;
									break;
								}

								if(check_comparison==ACCESS_IS_IN_SAME_STRIDE) {
									IF_DEBUG(printf("[SICA] THIS READ ACCESS ON '%s' IS ALREADY RECOGNIZED IN A PREVIOUS STRIDE AND THEREFORE NOT ADDED!\n", act_band->loop->stmts[s]->reads[r]->name););
									sica_access_is_new=0;
									break;
								}

								if(check_comparison==ACCESS_IS_NOT_IDENTICAL){
									IF_DEBUG(printf("[SICA] THIS READ ACCESS ON '%s' IS NEW!\n", act_band->loop->stmts[s]->reads[r]->name););
									act_access_temp=act_access_temp->next;
								}
							}

							//...IF NOT, add it to the linked list for this array and increase the counter ...
							if(sica_access_is_new)    {
								act_access_temp->nrows = act_band->loop->stmts[s]->reads[r]->mat->alloc_nrows;
								act_access_temp->ncols = act_band->loop->stmts[s]->reads[r]->mat->alloc_ncols;

								act_band->sicadata->vec_accesses++;
								//IF THIS ACCESS IS INNERMOST
								if(y==act_band->loop->stmts[s]->reads[r]->mat->alloc_nrows-1)    {
									act_band->sicadata->innermost_vec_accesses++;
								}

								act_access_temp->next=sica_accesses_malloc();

								act_access_temp->access_mat=sica_access_matrix_malloc(act_band->loop->stmts[s]->reads[r]->mat->alloc_nrows, act_band->loop->stmts[s]->reads[r]->mat->alloc_ncols);
								sica_copy_access_matrix(act_access_temp->access_mat, trans_access_mat, act_band->loop->stmts[s]->reads[r]->mat->alloc_nrows, act_band->loop->stmts[s]->reads[r]->mat->alloc_ncols);
								IF_DEBUG(printf("[SICA] ADDED READ ACCESS ON '%s'!\n", act_band->sicadata->id2arrayname[act_array_id]););

								int new_bytes=0;
								//IF THE DATA TYPE IS DEFINED
								if(act_band->loop->stmts[s]->reads[r]->symbol->data_type)    {
								IF_DEBUG(printf("[SICA] THIS ACCESS IS OF TYPE '%s'!\n", act_band->loop->stmts[s]->reads[r]->symbol->data_type););
								//COUNT the bytes that have to be loaded for this access
								new_bytes=sica_get_bytes_of_type(act_band->loop->stmts[s]->reads[r]->symbol->data_type);
								} else {
									printf("[SICA] WARNING: The datatype of an array was not recognized and therefore set to a DEFAULT VALUE: %i Bytes!\n", SICA_DEFAULT_DATA_BYTES);
									new_bytes=SICA_DEFAULT_DATA_BYTES;
								}
								//[SICA] update the largest datatype if necessary
								if(new_bytes>act_band->sicadata->largest_data_type){
									act_band->sicadata->largest_data_type=new_bytes;
								}
								//[SICA] add the additional bytes
								act_band->sicadata->bytes_per_vecit+=new_bytes;
							}

						}
						if(!act_band->sicadata->isvec)    {
							IF_DEBUG(printf("[SICA] THIS IS NOT A VECTORIZED BAND\n"););
						}

			    	}
		    	} else {
		    		IF_DEBUG(printf("[SICA] The Access on Array '%s' is NO array access!\n", act_band->loop->stmts[s]->reads[r]->name););
		    	}

		    }
		    //->STOP READ ANALYSIS


	    	//->START WRITE ANALYSIS
	    	IF_DEBUG2(printf("[SICA] Analyse the WRITE accesses for vectorization\n"););
	    	IF_DEBUG2(printf("[SICA] stmt=%i: nwrites=%i\n", s, act_band->loop->stmts[s]->nwrites););
		    for(w=0;w<act_band->loop->stmts[s]->nwrites;w++)
		    {
		    	IF_DEBUG2(printf("[SICA] write=%i, id: %i, name=%s, type=%s\n", w, sica_get_array_id(act_band, act_band->loop->stmts[s]->writes[w]->name), act_band->loop->stmts[s]->writes[w]->name, act_band->loop->stmts[s]->writes[w]->symbol->data_type););
		    	IF_DEBUG2(pluto_matrix_print(stdout, act_band->loop->stmts[s]->writes[w]->mat););

		    	//get the access matrix offset caused by tiling dimensions
		    	int access_offset=act_band->sicadata->transwidth;
		    	if(options->l2tile)    {
		    		access_offset=2*act_band->sicadata->transwidth;
		    	}

		    	//get the access matrix
		    	IF_DEBUG2(printf("[SICA] WRITE-ACCESS-MATRIX:\n"););
		    	IF_DEBUG2(sica_print_matrix_with_coloffset(act_band->loop->stmts[s]->writes[w]->mat->val, act_band->loop->stmts[s]->writes[w]->mat->alloc_nrows, act_band->sicadata->transwidth, access_offset););

		    	//go through all dimensions of this access and transform them
		    	int** orig_access_mat;
		    	int** trans_access_mat;

		    	//malloc the transformed mat to fill it
	    		orig_access_mat=(int**)malloc(act_band->loop->stmts[s]->writes[w]->mat->alloc_nrows*sizeof(int*));
	    		trans_access_mat=(int**)malloc(act_band->loop->stmts[s]->writes[w]->mat->alloc_nrows*sizeof(int*));

		    	for(y=0; y<act_band->loop->stmts[s]->writes[w]->mat->alloc_nrows; y++)    {
		    		orig_access_mat[y]=(int*)malloc(act_band->loop->stmts[s]->writes[w]->mat->alloc_ncols*sizeof(int));
		    		trans_access_mat[y]=(int*)malloc(act_band->loop->stmts[s]->writes[w]->mat->alloc_ncols*sizeof(int));
		    	}

		    	int orig_access_iterators[act_band->sicadata->transwidth];
		    	int trans_access_iterators[act_band->sicadata->transwidth];

		    	//fill the two access and row arrays
		    	for(y=0;y<act_band->loop->stmts[s]->writes[w]->mat->alloc_nrows;y++)    {

			    	for(x=0;x<act_band->loop->stmts[s]->writes[w]->mat->alloc_ncols;x++)    {
			    		orig_access_mat[y][x] = act_band->loop->stmts[s]->writes[w]->mat->val[y][x];
			    		trans_access_mat[y][x] = act_band->loop->stmts[s]->writes[w]->mat->val[y][x]; //after transformation overwrite the trans part
			    	}

			    	for(x=0;x<act_band->sicadata->transwidth;x++)    {
			    		orig_access_iterators[x] = act_band->loop->stmts[s]->writes[w]->mat->val[y][access_offset+x];
			    		trans_access_iterators[x] = 0;
				    	}

		    		sica_vec_times_matrix(trans_access_iterators,orig_access_iterators,act_band->sicadata->trans_inverted,act_band->sicadata->transwidth,act_band->sicadata->transwidth);

			    	//print the two access arrays
		    		//printf("ORIG-ACCESS:\t");
		    		//for(y=0;y<act_band->sicadata->transwidth;y++)    {
				    //		printf("%i ", orig_access[y]);
				    //	}
		    		//printf("\n");
                    //
		    		//printf("TRANS-ACCESS:\t");
		    		//for(y=0;y<act_band->sicadata->transwidth;y++)    {
				    //		printf("%i ", trans_access[y]);
				    //	}
		    		///printf("\n");

			    	//overwrite the transformed parts in the trans_row
			    	for(x=0;x<act_band->sicadata->transwidth;x++)    {
			    		trans_access_mat[y][access_offset+x] = trans_access_iterators[x];
			    	}
		    	}

		    	int act_array_id=sica_get_array_id(act_band, act_band->loop->stmts[s]->writes[w]->name);
		    	IF_DEBUG2(printf("[SICA] Looking up the following array name: '%s' with id: '%i' and transformed access matrix:\n", act_band->loop->stmts[s]->writes[w]->name, act_array_id););
		    	//This is now the transformed access
		    	IF_DEBUG2(sica_print_matrix_with_coloffset(trans_access_mat, act_band->loop->stmts[s]->writes[w]->mat->alloc_nrows, act_band->loop->stmts[s]->writes[w]->mat->alloc_ncols, 0););

		    	//Now we have all necessary data concerning this access, check if we have to count it for vectorized accesses!

		    	//check if the access is a just a scalar type, not a real array access (e.g. alpha or a[const][const])
		    	int entry_sum=sica_get_entry_sum(trans_access_mat, act_band->loop->stmts[s]->writes[w]->mat->alloc_nrows, act_band->loop->stmts[s]->writes[w]->mat->alloc_ncols);

		    	//IF it is a real array access...
		    	if(entry_sum)    {
		    		IF_DEBUG(printf("[SICA] The Access on Array '%s' is an array access!\n", act_band->loop->stmts[s]->writes[w]->name););
			    	for(y=0;y<act_band->loop->stmts[s]->writes[w]->mat->alloc_nrows;y++)    {

			    		//...CHECK IF there is one or more dimensions that is accessed by the vectorized loop and...
						if(act_band->sicadata->isvec&&trans_access_mat[y][access_offset+act_band->sicadata->vecrow])    {
							IF_DEBUG(printf("[SICA] VECTORIZATION: Array '%s' accesses dimension '%i' by a vectorized loop!\n", act_band->loop->stmts[s]->writes[w]->name, y););

							int sica_access_is_new=1;

							//...CHECK IF the access (matrix) is already recognized for this array (e.g. C[i][j]=C[i][j]+...). ...
							//printf("[SICA] ACT-POINTER: %p\n",sica_accesses_on_array[act_array_id]);
							SICAAccess* act_access_temp=sica_accesses_on_array[act_array_id];

							while(act_access_temp->next)    {
								//CHECK IF THE act_access_temp->mat is equal to the actual one
								int check_comparison=ACCESS_IS_NOT_IDENTICAL;

								check_comparison=sica_compare_access_matrices(trans_access_mat, act_access_temp->access_mat, act_band->loop->stmts[s]->writes[w]->mat->alloc_nrows, act_band->loop->stmts[s]->writes[w]->mat->alloc_ncols);

								//IF THEY ARE EQUAL
								if(check_comparison==ACCESS_IS_IDENTICAL)    {
									IF_DEBUG(printf("[SICA] THIS WRITE ACCESS ON '%s' IS ALREADY RECOGNIZED AND THEREFORE NOT ADDED!\n", act_band->loop->stmts[s]->writes[w]->name););
									sica_access_is_new=0;
									break;
								}

								if(check_comparison==ACCESS_IS_IN_SAME_STRIDE) {
									IF_DEBUG(printf("[SICA] THIS WRITE ACCESS ON '%s' IS ALREADY RECOGNIZED IN A PREVIOUS STRIDE AND THEREFORE NOT ADDED!\n", act_band->loop->stmts[s]->writes[w]->name););
									sica_access_is_new=0;
									break;
								}

								if(check_comparison==ACCESS_IS_NOT_IDENTICAL){
									IF_DEBUG(printf("[SICA] THIS WRITE ACCESS ON '%s' IS NEW!\n", act_band->loop->stmts[s]->writes[w]->name););
									act_access_temp=act_access_temp->next;
								}
							}

							//...IF NOT, add it to the linked list for this array and increase the counter ...
							if(sica_access_is_new)    {
								act_access_temp->nrows = act_band->loop->stmts[s]->writes[w]->mat->alloc_nrows;
								act_access_temp->ncols = act_band->loop->stmts[s]->writes[w]->mat->alloc_ncols;

								act_band->sicadata->vec_accesses++;
								//IF THIS ACCESS IS INNERMOST
								if(y==act_band->loop->stmts[s]->writes[w]->mat->alloc_nrows-1)    {
									act_band->sicadata->innermost_vec_accesses++;
								}

								act_access_temp->next=sica_accesses_malloc();

								act_access_temp->access_mat=sica_access_matrix_malloc(act_band->loop->stmts[s]->writes[w]->mat->alloc_nrows, act_band->loop->stmts[s]->writes[w]->mat->alloc_ncols);
								sica_copy_access_matrix(act_access_temp->access_mat, trans_access_mat, act_band->loop->stmts[s]->writes[w]->mat->alloc_nrows, act_band->loop->stmts[s]->writes[w]->mat->alloc_ncols);
								IF_DEBUG(printf("[SICA] ADDED WRITE ACCESS ON '%s'!\n", act_band->sicadata->id2arrayname[act_array_id]););

								int new_bytes=0;
								//IF THE DATA TYPE IS DEFINED
								if(act_band->loop->stmts[s]->writes[w]->symbol->data_type)    {
								IF_DEBUG(printf("[SICA] THIS ACCESS IS OF TYPE '%s'!\n", act_band->loop->stmts[s]->writes[w]->symbol->data_type););
								//COUNT the bytes that have to be loaded for this access
								int new_bytes=sica_get_bytes_of_type(act_band->loop->stmts[s]->writes[w]->symbol->data_type);
								} else {
									printf("[SICA] WARNING: The datatype of an array was not recognized and therefore set to a DEFAULT VALUE: %i Bytes!\n", SICA_DEFAULT_DATA_BYTES);
									new_bytes=SICA_DEFAULT_DATA_BYTES;
								}
								//[SICA] update the largest datatype if necessary
								if(new_bytes>act_band->sicadata->largest_data_type){
									act_band->sicadata->largest_data_type=new_bytes;
								}
								//[SICA] add the additional bytes
								act_band->sicadata->bytes_per_vecit+=new_bytes;
							}

						}
						if(!act_band->sicadata->isvec)    {
							IF_DEBUG(printf("[SICA] THIS IS NOT A VECTORIZED BAND\n"););
						}

			    	}
		    	} else {
		    		IF_DEBUG(printf("[SICA] The Access on Array '%s' is NO array access!\n", act_band->loop->stmts[s]->writes[w]->name););
		    	}

		    }
	    	//->STOP WRITE ANALYSIS


	    } /* End of loop across the arrays */

	    IF_DEBUG(printf("[SICA] There are %i accesses relevant for vectorization\n", act_band->sicadata->vec_accesses););

	    IF_DEBUG(printf("[SICA] Print the accesses on arrays structure:\n"););
	    IF_DEBUG(sica_print_array_accesses_structures(act_band, sica_accesses_on_array););
	    //get the PluTo defined data types + (null)->default and calculate the tile quantities

	    //NOW LOKK UP HOW MANY ACCESSES OF WHICH TYPE ARE AVAILABLE



		///////////////////////////////////////////////
}


void sica_print_matrix_with_coloffset(int** matrix, int rows, int cols, int coloffset)    {
	int x,y;

	for(x=0;x<rows;x++)    {
		for(y=0;y<cols;y++)    {
			printf("%i ", matrix[x][coloffset+y]);
		}
		printf("\n");
	}
	printf("\n");
}

void sica_print_array_accesses_structures(Band* act_band, SICAAccess** sica_accesses_on_array)    {
	int a;
	int counter;
	for(a=0; a<act_band->sicadata->nb_arrays; a++)    {
	printf("[SICA] \tArray '%s': \n",act_band->sicadata->id2arrayname[a]);
	SICAAccess* act_access_temp=sica_accesses_on_array[a];
	counter=0;
	while(act_access_temp->next)    {
		printf("[SICA] \t\t%p:\n", act_access_temp);
		act_access_temp=act_access_temp->next;
		counter++;
	}
	printf("[SICA] \t\t%i access at all on this array in this band.\n", counter);
}
}


