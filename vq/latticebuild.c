/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE Ogg Vorbis SOFTWARE CODEC SOURCE CODE.  *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS SOURCE IS GOVERNED BY *
 * THE GNU PUBLIC LICENSE 2, WHICH IS INCLUDED WITH THIS SOURCE.    *
 * PLEASE READ THESE TERMS DISTRIBUTING.                            *
 *                                                                  *
 * THE OggSQUISH SOURCE CODE IS (C) COPYRIGHT 1994-2000             *
 * by Monty <monty@xiph.org> and The XIPHOPHORUS Company            *
 * http://www.xiph.org/                                             *
 *                                                                  *
 ********************************************************************

 function: utility main for building codebooks from lattice descriptions
 last mod: $Id: latticebuild.c,v 1.4 2000/07/17 12:55:37 xiphmont Exp $

 ********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include "vorbis/codebook.h"
#include "../lib/sharedbook.h"
#include "bookutil.h"

/* The purpose of this util is just to finish packaging the
   description into a static codebook.  It used to count hits for a
   histogram, but I've divorced that out to add some flexibility (it
   currently generates an equal probability codebook) 

   command line:
   latticebuild description.vql

   the lattice description file contains two lines:

   <n> <dim> <multiplicitavep> <sequentialp>
   <value_0> <value_1> <value_2> ... <value_n-1>
   
   a threshmap (or pigeonmap) struct is generated by latticehint;
   there are fun tricks one can do with the threshmap and cascades,
   but the utils don't know them...

   entropy encoding is done by feeding an entry list collected from a
   training set and feeding it to latticetune along with the book.

   latticebuild produces a codebook on stdout */

static int ilog(unsigned int v){
  int ret=0;
  while(v){
    ret++;
    v>>=1;
  }
  return(ret);
}

int main(int argc,char *argv[]){
  codebook b;
  static_codebook c;
  double *quantlist;
  long *hits;

  int entries=-1,dim=-1,quantvals=-1,addmul=-1,sequencep=0;
  FILE *in=NULL;
  char *line,*name;
  long i,j;

  memset(&b,0,sizeof(b));
  memset(&c,0,sizeof(c));

  if(argv[1]==NULL){
    fprintf(stderr,"Need a lattice description file on the command line.\n");
    exit(1);
  }

  {
    char *ptr;
    char *filename=calloc(strlen(argv[1])+4,1);

    strcpy(filename,argv[1]);
    in=fopen(filename,"r");
    if(!in){
      fprintf(stderr,"Could not open input file %s\n",filename);
      exit(1);
    }
    
    ptr=strrchr(filename,'.');
    if(ptr){
      *ptr='\0';
      name=strdup(filename);
    }else{
      name=strdup(filename);
    }

  }
  
  /* read the description */
  line=get_line(in);
  if(sscanf(line,"%d %d %d %d",&quantvals,&dim,&addmul,&sequencep)!=4){
    if(sscanf(line,"%d %d %d",&quantvals,&dim,&addmul)!=3){
      fprintf(stderr,"Syntax error reading book file (line 1)\n");
      exit(1);
    }
  }
  entries=pow(quantvals,dim);
  c.dim=dim;
  c.entries=entries;
  c.lengthlist=malloc(entries*sizeof(long));
  c.maptype=1;
  c.q_sequencep=sequencep;
  c.quantlist=calloc(quantvals,sizeof(long));

  quantlist=malloc(sizeof(long)*c.dim*c.entries);
  hits=malloc(c.entries*sizeof(long));
  for(j=0;j<entries;j++)hits[j]=1;
  for(j=0;j<entries;j++)c.lengthlist[j]=1;

  reset_next_value();
  setup_line(in);
  for(j=0;j<quantvals;j++){  
    if(get_line_value(in,quantlist+j)==-1){
      fprintf(stderr,"Ran out of data on line 2 of description file\n");
      exit(1);
    }
  }

  /* gen a real quant list from the more easily human-grokked input */
  {
    double min=quantlist[0];
    double mindel=-1;
    int fac=1;
    for(j=1;j<quantvals;j++)if(quantlist[j]<min)min=quantlist[j];
    for(j=0;j<quantvals;j++)
      if(min!=quantlist[j] && (mindel==-1 || quantlist[j]-min<mindel))
	mindel=quantlist[j]-min;

    fprintf(stderr,"min=%g mindel=%g\n",min,mindel);
    j=0;
    while(j<quantvals){
      for(j=0;j<quantvals;j++){
	double test=(quantlist[j]-min)/(mindel/fac);
	if( fabs(rint(test)-test)>.000001) break;
      }
      if(j<quantvals)fac++;
    }

    mindel/=fac;

    c.q_min=_float32_pack(min);
    c.q_delta=_float32_pack(mindel);
    c.q_quant=0;

    min=_float32_unpack(c.q_min);
    mindel=_float32_unpack(c.q_delta);
    for(j=0;j<quantvals;j++){
      c.quantlist[j]=rint((quantlist[j]-min)/mindel);
      if(ilog(c.quantlist[j])>c.q_quant)c.q_quant=ilog(c.quantlist[j]);
    }
  }

  /* build the [default] codeword lengths */
  memset(c.lengthlist,0,sizeof(long)*entries);
  for(i=0;i<entries;i++)hits[i]=1;
  build_tree_from_lengths(entries,hits,c.lengthlist);

  /* save the book in C header form */
  write_codebook(stdout,name,&c);
  fprintf(stderr,"\r                                                     "
	  "\nDone.\n");
  exit(0);
}
