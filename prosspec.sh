#! /bin/sh
################### Start of $RCSfile: prosspec.sh,v $ ##################
#
# $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/prosspec.sh,v $
# $Id: prosspec.sh,v 1.2 2004/07/08 20:34:45 alb Exp alb $
# $Date: 2004/07/08 20:34:45 $
# $Author: alb $
#
#
####### description ################################################
#
#
#
####################################################################

OS="`uname -s`"

if [ $OS = "AIX" ] ; then
  VERSION=`uname -vr`
else
  VERSION=`uname -r`
fi

OSSPEC="$OS $VERSION"
 
export OSSPEC

echo $OSSPEC

