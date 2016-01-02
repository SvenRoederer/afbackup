#! /bin/sh
################### Start of $RCSfile: prosname.sh,v $ ##################
#
# $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/prosname.sh,v $
# $Id: prosname.sh,v 1.2 2004/07/08 20:34:45 alb Exp alb $
# $Date: 2004/07/08 20:34:45 $
# $Author: alb $
#
#
####### description ################################################
#
#
#
####################################################################

OS="`uname -s|sed 's/[^A-Za-z0-9]//g'`"

if [ $OS = "AIX" ] ; then
  VERSION=`uname -v|sed 's/[.][0-9].*//g'`
else
  if [ $OS = "HPUX" -o $OS = "OSF1" ] ; then
    VERSION=`uname -r|sed 's/^[^0-9]*//g;s/[.][0-9].*//g'`
  else
    VERSION=`uname -r|sed 's/[.][0-9].*//g'`
  fi
fi
VERSION=`echo $VERSION|sed 's/^0*//g'`

OSNAME=$OS"_"$VERSION
 
export OSNAME

echo $OSNAME

