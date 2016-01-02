#!/usr/bin/perl
#
# This script requires the following configuration or needs to
# be adjusted to work with other settings:
#
# The path to perl in line # 1 is /usr/bin/perl
# afbackup client basedir is /usr/local/backup/client
# client var directory is /usr/local/backup/client/var
# index processing program is gzip [ -whatever ]

$afclntdir = "/usr/local/backup/client";
$afclntbindir = "$afclntdir/bin";
$afclntvardir = "$afclntdir/var";
$idxunprocesscmd = "zcat";

require 'getopts.pl';
if (!Getopts("c:d:a") || !defined $ARGV[0]) {
  print "Usage: restore.pl [-c dump] [-d directory] [-a] string\n";
  print "   Restore file or directory from afbackup tapes\n";
  print "       string       the string to search for in file names\n";
  print "    -c dump         restrict to a single dump, 0 latest, 1 previous etc\n";
  print "    -d directory    directory to restore to\n";
  print "    -a              search in full path instead of just file name\n";
  exit 1;
}

$rdir = $opt_d;
$cycle = $opt_c;
$search = $ARGV[0];
if ($opt_a) {
  $searchall = 1;
} else {
  if ($search =~ /\//) {
    $searchall = 1;
  } else {
    $searchall = 0;
  }
}
sub printl {
	print "$i\t$atape[$i]\t$atime[$i]\t$afile[$i]\n";
}

sub fetch {
  my ($i) = @_;
  my ($hostn,$hostp) = split(/\%/,$ahost[$i]);
  my ($tapen,$tapef) = split(/\./,$atape[$i]);
  chdir $rdir;
  print "$afclntbindir/afbackup -xvgu -h $hostn -p $hostp -C $tapen -F $tapef -W cyllene.uwa.edu.au -r $afile[$i]\n";
  system "$afclntbindir/afbackup -xvgu -h $hostn -p $hostp -C $tapen -F $tapef -W cyllene.uwa.edu.au -r $afile[$i]";
  chdir $afclntvardir;
}

chdir $afclntvardir;
$j = 0;
while (<backup_log*.z>) {
  $index[$j] = $_;
  $j++;
}

$i = 0;
for ($j = 0; $j <= $#index ; $j++) {
  if (!defined $cycle || $j == ($#index - $cycle)) {
    print "Index=$index[$j]\n";
    open(W,"$idxunprocesscmd $index[$j]|");
    while (<W>) {
      if (/^(.+)\!([0-9\.]+)\|(\d+)\~(\d+): (.+)$/) {
	$host=$1;
	$tape=$2;
#	$num=$3;
	$time=$4;
	$file=$5;
	if ($searchall) {
	  $ffile = $file;
	} else {
	  if ($file =~ /\/([^\/]+)$/) {
	    $ffile = $1;
	  } else {
	    $ffile = $file;
	  }
	}
	if ($ffile =~ /$search/) {
	  $ctime = localtime($time);
	  $ahost[$i] = $host;
	  $atape[$i] = $tape;
	  $afile[$i] = $file;
	  $atime[$i] = $ctime;
	  printl();
	  $i++;
	}
      }
    }
  }
}

print "Enter index: ";
$result = <STDIN>;
fetch($result);

