#!/usr/bin/perl
#
# simple_tape_barcodes.pl [tape_no]
#
# Find location of tapes using mtx and tell
#  afbackup about them using cart_ctl
# if tape_no is set load that tape
#
# Summary:
# Perl program for handling labeled tapes
# We have a tape robot that uses barcode tape labels, this
# is perl program I have that makes this work with afbackup.
#
# Basically I use the last two digits from the tape label
# as the afbackup cartridge number and this program
# reads the output of mtx which shows the tape labels
# and uses this to set the cart_ctl info about where the
# tapes are. It will also optionally loads a cartridge
# by number.
#
# The program requires a few variables to be set
# to work at other locations.

$tape = $ARGV[0];

$pattern = 'CK73(\d+)L1';                      # Extract tape number from label
$range = '55-70';                              # Range of tape numbers
$location = '/usr/local/backup';               # afbackup install location
$cart_ctl = "$location/server/bin/cart_ctl";   # cart_ctl command
$mtx = '/usr/sbin/mtx -f /dev/sg1';            # mtx enquiry command

sub scan_tapes {
  undef %Tnumber;
  print "$mtx status\n";
  open (M,"$mtx status|") || die "Cann't run mtx: $!";
  while (<M>) {
    if (/Data Transfer Element (\d+)/ || /Storage Element (\d+)/) {
      $Snumber = $1;
      if (/VolumeTag\s*=\s*$pattern/) {
	$Tnumber{$Snumber} = $1;
	#      print "$Snumber=$Tnumber{$Snumber}\n";
      
      }
    }
  }
  close M;
}

print "$cart_ctl -P -C $range -P ''\n";
`$cart_ctl -P -C $range -P ''`;

scan_tapes();

if ($tape) {
  if ( $Tnumber{'0'} eq $tape) {
    $loaded = 1;
  } elsif ( $Tnumber{'0'} ne $tape) {
    if ($Tnumber{'0'}) {
      print "$mtx unload\n";
      `$mtx unload`;
    }
    foreach $i (keys %Tnumber) {
      if ($Tnumber{$i} eq $tape) {
	print "$mtx load $i\n";
	`$mtx load $i`;
	$loaded = 1;
      }
    }
  }
}
    
scan_tapes();

foreach $i (keys %Tnumber) {
  if ($i == 0) {
    print "$cart_ctl -P -D -C $Tnumber{$i}\n";
    `$cart_ctl -P -D -C $Tnumber{$i}`;
  } else {
    print "$cart_ctl -P -S $i -C $Tnumber{$i}\n";
    `$cart_ctl -P -S $i -C $Tnumber{$i}`;
  }
}

if ($tape && !$loaded) {
  die "Cann't load tape: $tape";
}
