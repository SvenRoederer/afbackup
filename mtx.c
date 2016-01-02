/* mtx.c

This program was written to control the ADIC 1200G autoloading DAT drive.
It may also work on other drives, but take into consideration the fact
that I am new at SCSI programming and I put this together in a few days to
test the device. Try it and let me know of successes/failures.

This program was written (and it seem to work) on a HP9000/735 with HP-UX
9.01. If you have HP-UX 10.x you can find a similar program in patch
PHCO_10848.

It needs the Autochanger driver compiled in the kernel, by default it's
not, so you'll have to rebuild the kernel.

It needs to know BOTH the device file for the Autoloader and for the Tape
as explained in the comments (See the Unload Function). Set the default
values in the proper variables later. If you know a way of having it work
without knowing the tape too, please let me know as it bugged me no end. 

It is important that the autochanger device is created with the proper
major number (on HP-UX 9.x it's 33, check it in /etc/master) 

Syntax is explained in the "Usage:" section that is printed when mtx is
run without parameters. I hope it's clear enough not to need any further
explanation.

In case of problems it will exit with status 1 and print a (hopefully)
explanatory message. 


(c) 1998 GiP, (GianPiero Puccioni)gip@ino.it
Permission to use, copy, modify, distribute, and sell this software and
its documentation for any purpose is hereby granted without fee, provided
that the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.  The author makes no representations about the suitability
of this software for any purpose.It is provided "as is" without express or
implied warranty. 
*/


#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/scsi.h>

#define MAXSLOT 16

struct element_addresses el_ad;    
struct element_status el_st;    
struct move_medium_parms move;
struct inquiry_2 inq;
struct reservation_parms res;
char load[2][6]={"Empty","Full"};
char errmsg[80];
int slot[MAXSLOT+1];
int ns,fs,fd,ft,from;
int dev,rc;
int last,first;
char *prog;

int SCSI_Open(void);
void SCSI_Close(void);
void Ferror(void);
void Help(void); 
void Init(void);
void PrintStatus(void);
void Load(int sl);
void MtOff(void);
void Unload(int sl);
void Next(int i);
void Prev(int i);


/* defaults for Autochanger and Tape device files */
/* Set your own */
char chgf[50]="/dev/scsi/adic";
char tapf[50]="/dev/rmt/dat";



int main(int argc, char **argv)
{
int i,j,ch;
char action;
int where;
int numopt=0;

/* collect the options */

prog=argv[0];
action='z';
while ((ch = getopt(argc, argv, ":hsl:u:ipnf:t:")) != EOF)
{
   switch(ch)
   {
      case 'h' : action='h';                 /* help */ 
                 numopt++;
                 break;
      case 's' : action='s';                 /* status */
                 numopt++;
                 break;
      case 'u' : action='u';                 /* unload */
                 numopt++;
                 sscanf(optarg,"%d",&where);
                 break;
      case 'l' : action='l';                 /* load */
                 numopt++;
                 sscanf(optarg,"%d",&where);
                 break;
      case 'n' : action='n';                 /* next */
                 numopt++;
                 break;
      case 'p' : action='p';                 /* prev */
                 numopt++;
                 break;
      case 'i' : action='i';                 /* init */
                 numopt++;
                 break;
      case 'f' : strncpy(chgf,optarg,50) ;   /* set changer */
                 break;
      case 't' : strncpy(tapf,optarg,50) ;   /* set tape */
                 break;
      case ':' : if (optopt == 'u')      /* 'u' without parms */ 
                 {   where=0;  
                     numopt++;
                     action='u';
                     break;
                 }
                 printf("option -%c requires argument \n",optopt);
                 Help();
                 break;
      case '?' : printf("option -%c is unrecognised\n",optopt);
                 Help();
                 exit(1);
                 break;
  }
}

/* Too many options Print Usage:*/

if (numopt > 1)
{ printf(" Too many Options ! \n");
  Help();
  exit(1);
}

/* Either no parms or 'h' => Print "Usage:" */

if(action == 'z'||action == 'h')
{  Help();
   exit(0);
}

/* Open Autochanger device */
dev=SCSI_Open();


/* Check if it's recognized as a CHANGER  */
rc = ioctl(dev,SIOC_INQUIRY,&inq);
if (rc) 
{    sprintf(errmsg,"ioctl INQUIRY failed: %d\n",errno);
     Ferror();
} 
else if(inq.dev_type!= SCSI_AUTOCHANGER )
{
  sprintf(errmsg,"Wrong dev type: %d\n",inq.dev_type);
     Ferror();
}

/* Get (internal) address of elements */
rc = ioctl(dev,SIOC_ELEMENT_ADDRESSES,&el_ad);
if (rc) 
{    fprintf(stderr,"ioctl failed: %d\n",errno);
     close(dev);
     exit(1);
} 
else
{
fs=el_ad.first_storage;
ns=el_ad.num_storages;
fd=el_ad.first_data_transfer;
ft=el_ad.first_transport;
first=fs;            /* address of first element */
last=fs+ns-1;        /* address of last element */ 

}
if (ns > MAXSLOT)   /* just in case... */
{    sprintf(errmsg,"Too many slots: %d MAX=%d\n",ns,MAXSLOT);
     SCSI_Close();
     Ferror();
}


/* get status of transfer element (i.e. tape) 
   In case it's loaded get address of element loaded. */
from=0;
el_st.element=fd;
rc = ioctl(dev,SIOC_ELEMENT_STATUS,&el_st);
slot[0]=el_st.full;
if(slot[0]) 
   from=el_st.source_element;

/* get status of each slot (Empty or Full) 
   First Slot is 1
*/
for(i=1,j=fs;i<=ns;i++,j++)
{  el_st.element=j;
   rc = ioctl(dev,SIOC_ELEMENT_STATUS,&el_st);
   if (rc) 
   {    sprintf(errmsg,"ioctl STAT failed: %d\n",errno);
        SCSI_Close();
        Ferror();
   } 
slot[i]=el_st.full;
}

/* Do the job */
switch(action)
{
   case 'i': Init();
             break;
   case 's': PrintStatus();
             break;
   case 'l': Load(where);
             break;
   case 'u': Unload(where);
             break;
   case 'n': Next(0);
             break;
   case 'p': Prev(0);
             break;
   case 'N': Next(1);
             break;
   case 'P': Prev(1);
             break;
}


SCSI_Close();
exit(0);

}
/* End Of Main */


/* Open the SCSI device for the changer */

int SCSI_Open()
{
int d;
d = open(chgf, O_RDONLY);
if (d < 0)
{  sprintf(errmsg,"Cannot open SCSI device '%s'\n", chgf);
   Ferror();
}
return d;
}

/* Close the SCSI device for the changer */

void SCSI_Close()
{
  if (close(dev) < 0)
  { sprintf(errmsg,"Cannot close SCSI device '%s'\n", chgf);
    Ferror();
  }
}

/* Print the Error message and exit with 1 */

void Ferror()
{
fprintf(stderr,errmsg);
exit(1);
}


/* Print "Usage:" message */

void Help() 
{
printf ("\n Usage: %s [-f changer] [-t tape] option\n",prog);
printf ("   option: -s     print status\n");
printf ("           -lN    load from slot N (first slot is 1)\n");
printf ("           -u[N]  unload to N or to slot last loaded\n");
printf ("           -n     load from next available slot\n");
printf ("           -N     like -n but if Last tape loaded load First\n");
printf ("           -p     load from previous available slot\n");
printf ("           -P     like -p but if First tape loaded load Last\n");
printf ("           -i     init device\n");
printf ("           -h     print this help\n\n");
printf ("   if changer is not specified %s will be used\n",chgf);
printf ("   if tape is not specified %s will be used\n",tapf);
printf ("   if media is loaded a 'load' request will imply a -u\n\n");
exit(0);
}


/* Init the changer:
   Test each slot for presence of a tape 
   It will fail if a tape is in the driver 
*/

void Init()
{

if(slot[0])    
{    printf("Cannot Initialize: a tape is loaded\n");
     exit(1);
} 

rc = ioctl(dev,SIOC_INIT_ELEM_STAT,&el_ad);
if (rc) 
{    sprintf(errmsg,"ioctl INIT failed: %d\n",errno);
     Ferror();
} 
}

/* Print the status of the changer:
   the values of changer and tape devices
   if media is loaded (and from where it came)
   if each slot is Empty or Full 
*/

void PrintStatus()
{
int i;

printf("Changer:%s\n",chgf);
printf("Tape   :%s\n",tapf);

if(slot[0])
 printf(" Data element: Full (Element %d loaded)\n",from-fs+1);
else
 printf(" Data element: Empty\n");

for(i=1;i<=ns;i++)
  printf(" Slot %2d: %s\n", i,load[slot[i]]);
}

/* Load media from slot sl 
   if a tape is already loaded unload it 
   to the slot it came from
*/

void Load(int sl)
{

if(slot[0])    
  Unload(0);

move.transport=ft;
move.source=fs+sl-1;
move.destination=fd;
move.invert=0;
rc = ioctl(dev,SIOC_MOVE_MEDIUM,&move);
if (rc) 
{    sprintf(errmsg,"load failed: errno=%d\n",errno);
     SCSI_Close();
     Ferror();
} 
}

/* Load Next Tape 
   if k=1 then last=>first
*/

void Next(int k)
{
if (from == last)
{  
   if(!k)
   {  sprintf(errmsg,"Last Tape loaded\n",errno);
      SCSI_Close();
      Ferror();
   }
   Load(first);
} 
else
   Load(from+1);
}

/* Load Prev Tape 
   if k=1 then first=>last
*/

void Prev(int k)
{

if (from == first)
{  
   if(!k)
   {  sprintf(errmsg,"First Tape loaded\n",errno);
      SCSI_Close();
      Ferror();
   }
   Load(last);
} 
else
   Load(from-1);
}


/* Unload a tape !

This was the big problem and the reason why I need to know the device for the
tape too. What happens is that when I load a tape the Autochanger goes into a
"locked" status and I couldn't find a way of "unlocking" it. According to the
SCSI docs a "move" out of the transfer element should unmount the media, but
it doesn't. Only way I could find to do this is send the equivalent of a 
"mt -t tape rewoffl" to the TAPE device. If you know how to do this some other
way let me know.
*/

void Unload(int sl)
{

if (!slot[0])
{  printf("No tape loaded\n");
   exit(0);
}

/* unload to original slot or to selected slot */
if (sl == 0)
  sl=from;
else
  sl=sl+fs-1;

/* I don't know if it makes sense to unload to a different slot. It seems
it's supported by the SCSI but I don't think the robot can do it. 
So I disabled it for now.

If you want to try comment out the following line
*/
sl=from;    /* unload from the original slot anyway */

MtOff();    /* this will rewind the tape and put it offline */

move.transport=ft;
move.source=fd;
move.destination=sl;
move.invert=0;
rc = ioctl(dev,SIOC_MOVE_MEDIUM,&move);
if (rc) 
{    sprintf(errmsg,"unload failed: errno=%d\n",errno);
     SCSI_Close();
     Ferror();
} 

}


/* Tape stuff:

   Open the device, send the command to rewind it and put it offline
   then close the device.
   
   If the device can't be opened it waits a little and tries again,
   sometimes it happens when two commands are given one after the other 
*/
 
#include <sys/mtio.h>
void MtOff()
{
int devd,rc;
struct mtop mop;    

mop.mt_op=MTOFFL;
mop.mt_count=1;

devd = open(tapf,O_RDWR);
if (devd < 0)
{   printf("Wait....\n");
    sleep(5);    
    devd = open(tapf,O_RDWR);
    if (devd < 0)
    {  sprintf(errmsg,"cannot open DAT device:%d  errno=%d \n",devd, errno);
       Ferror();
       exit(1);
    }
}

rc = ioctl(devd,MTIOCTOP,&mop);
if (rc) 
{    sprintf(errmsg,"Offline failed: errno=%d\n",errno);
     close(devd);
     SCSI_Close();
     Ferror();
     exit(1);
} 
close(devd);
}
