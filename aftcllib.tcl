################### Start of $RCSfile: aftcllib.tcl,v $ ##################
#
# $Source: /home/alb/afbackup/afbackup-3.5.8.12/RCS/aftcllib.tcl,v $
# $Id: aftcllib.tcl,v 1.3 2008/11/17 20:09:37 alb Exp alb $
# $Date: 2008/11/17 20:09:37 $
# $Author: alb $
#
#
####### description ################################################
#
#
#
####################################################################

set GVar_errorProc GGUI_errorDialog

#
# Post error message to the user, either through
# an open GUI or to the command line
#
proc GIO_errorMessage { msg } {
  global	GVar_errorProc

  eval { $GVar_errorProc $msg }
}

#
# Post error message to stderr of command line
#
proc GIO_errorOutput { message } {
  puts stderr "Error: $message"
}

#
# read contents of a file with name filename (first arg)
# into variable named as 2nd arg. Depent on the mode (default: array)
# store into the variable as:
#
#  mode        explanation
#
#  array       array with indexes starting with 0
#  list        a TCL-list
#  string      as a string with newline characters between the lines
#  words       as string with space characters between lines
#
# number of read lines is stored in variable named in 3rd arg.
# returns != 0 on error
#
proc GFile_readFile { filename linesptr num_linesptr { mode "array" } } {
  upvar $linesptr lines
  upvar $num_linesptr num_lines

  set mode [ string tolower $mode ]

  set errfl [ catch { set fp [ open $filename r ] } ]

  if { $errfl } {
    return 1
  }

  catch { unset lines }
  switch $mode {
    "list" {
      set lines ""
    }
    "string" {
      set lines ""
    }
    "words" {
      set lines ""
    }
  }

  set num_lines 0
  set i 0
  switch $mode {
    "array" {
      while { [ gets $fp lines($i) ] >= 0 } {
	incr i
      }
    }
    "list" {
      while { [ gets $fp line ] >= 0 } {
	lappend lines $line
	incr i
      }
    }
    "string" {
      while { [ gets $fp line ] >= 0 } {
	incr i
	append lines "${line}\n"
      }
    }
    "words" {
      while { [ gets $fp line ] >= 0 } {
	incr i
	if { $lines != "" } {
	  append lines " "
	}
	append lines "${line}"
      }
    }
  }

  close $fp

  set num_lines $i

  return 0
}

#
# write contents of a variable named as 2nd arg into a file
# named in 1st arg. Number of lines is given in 3rd arg. Write
# dependent on the mode (default: array) as:
#
#  mode     explanation
#
#  array    each element of the array starting with index 0
#  list     each element of the list
#  string   simply write 2nd arg to file
#  words    one word in 2nd arg per line
#  
proc GFile_writeFile { filename linesptr num_lines { mode "array" } } {
  upvar $linesptr lines

  set mode [ string tolower $mode ]

  set openfile 1

  if { $filename == "-" || $filename == "--" } {
    set openfile 0
    if { $filename == "-" } {
      set fp stdout
    } else {
      set fp stderr
    }
  }

  if { $openfile } {
    set errfl [ catch { set fp [ open $filename w ] } ]

    if { $errfl } {
      return 1
    }
  }

  switch $mode {
    "array" {
      for { set i 0 } { $i < $num_lines } { incr i } {
	puts $fp $lines($i)
      }
    }
    "list" {
      foreach elem $lines {
	puts $fp $elem
      }
    }
    "string" {
      puts -nonewline $fp $lines
    }
    "words" {
      foreach elem $lines {
	puts $fp $elem
      }
    }
  }

  if { $openfile } {
    close $fp
  }

  return 0
}

#
# Read file named in 1st arg, put each line into another element
# of the resulting TCL-list. When discovered, the include_inst
# serves as include-directive. The file named thereafter will be
# used for further input. Recursions are allowed. The global
# variable Conf_includePath can be supplied as TCL-list. Returns
# lines as TCL-list
#
proc GFile_readFilesAsListRecursive { filename { include_inst "#include" } } {
  global	Conf_includePath env

  set include_inst_last [ expr [ string length $include_inst ] - 1 ]

  set list ""

  set filename [ GFile_findFileInPathVar $filename Conf_includePath ]
  set failed [ catch { set fp [ open $filename r ] } ]
  if { $failed } {
    puts stderr "Warning: cannot open file $filename."
    return ""
  }

  while { [ gets $fp line ] >= 0 } {
    if { [ string range $line 0 $include_inst_last ] == $include_inst } {
      foreach inc_file [ lrange $line 1 end ] {
	set inc_file [ GFile_findFileInPathVar $inc_file Conf_includePath ]
	set inclist [ GFile_readFilesAsListRecursive $inc_file $include_inst ]
	foreach elem $inclist {
	  lappend list $elem
	}
      }
    } else {
      lappend list $line
    }
  }

  close $fp

  return $list
}

#
# Split lines in the array, whose name is supplied as 1st arg, info fields.
# Field separator is space or optional 5th arg. 2nd arg tells the number of
# lines in array arg 1. 3rd arg is name of target two-dimensional array var.
# First index: line number, second index: field number, each index starts
# with 0. 4th arg supplies the number of fields, that should be in each line.
# If number does not fit, an error message is posted. If 6th arg is given,
# error is not posted and line is ignored, if the line matches the regular
# expression in the 6th arg
#
proc GFmCv_linesToFields { linesptr num_lines fieldsptr num_fields { separator " " } { ignore_pattern "" } } {
  upvar $linesptr lines
  upvar $fieldsptr fields

  set seplen [ string length $separator ]

  incr num_fields -1

  for { set i 0 } { $i < $num_lines } { incr i } {
    set actline $lines($i)

    set rest $actline

    for { set j 0 } { $j < $num_fields } { incr j } {
      set idx [ string first $separator $rest ]
      if { $idx < 0 } {
	set txt "Format error in the following line:\n$actline"
	if { $ignore_pattern == "" || ! [ regexp $ignore_pattern $actline ] } {
	  GIO_errorMessage $txt
	}
	set idx 0
      }

      set fields($i,$j) [ string range $rest 0 [ expr $idx - 1 ] ]

      set rest [ string range $rest [ expr $idx + $seplen ] end ]
    }

    set fields($i,$j) $rest
  }
}

proc GFmCv_lineToFieldsVar { line fieldsptr separator { keep_empty_fields 0 } } {
  upvar $fieldsptr fields

  set num 0
  set rest $line
  set idx [ string first $separator $line ]
  set seplen [ string length $separator ]
  if { [ string tolower $keep_empty_fields ] == "false" } {
    set keep_empty_fields 0
  }

  while { $idx >= 0 } {
    set fields($num) [ string range $rest 0 [ expr $idx - 1 ] ]

    if { [ string length [ string trim $fields($num) ] ] != 0 || $keep_empty_fields != 0 } {
      incr num
    }

    set rest [ string range $rest [ expr $idx + $seplen ] end ]
    set idx [ string first $separator $rest ]
  }

  set fields($num) $rest

  if { (($num != 0 || $line != "") && [ string length [ string trim $fields($num) ] ] != 0) || $keep_empty_fields != 0 } {
    incr num
  }

  return $num
}

#
# Convert array named in first arg into TCL-list
# 2nd arg tells number of elements in array, 1st index is 0
#
proc GTyCv_arrayToList { arr num } {
  upvar $arr array

  set list ""
  for { set i 0 } { $i < $num } { incr i } {
    lappend list $array($i)
  }

  return $list
}

#
# Convert TCL-list in first arg into array named in
# 2nd arg. 1st array index is 0
#
proc GTyCv_listToArray { list arr } {
  upvar $arr array

  catch { unset array }

  set i 0
  foreach elem $list {
    set array($i) $elem
    incr i
  }

  return $i
}

#
# Reduce array of lists to array of list elements, whose
# index is given in 3rd arg. Num gives number of elements
# in array starting with 0
#
proc GArr_cutField { arr num idx } {
  upvar $arr array

  for { set i 0 } { $i < $num } { incr i } {
    set array($i) [ lindex $array($i) $idx ]
  }
}

#
# concatenate list elements to a string separating
# the list elements with space characters
#
proc GTyCv_listToString { list } {
  set string ""

  foreach elem $list {
    if { [ string trim $elem ] != "" } {
      if { $string != "" } {
	append string " "
      }
      append string $elem
    }
  }

  return $string
}

#
# concatenate array elements to a string separating
# the elements with space characters. 1st arg names
# the array, 2nd arg gives number of elements in
# array starting with index 0
#
proc GTyCv_arrayToString { arr num } {
  upvar $arr array

  set string ""
  for { set i 0 } { $i < $num } { incr i } {
    if { [ string trim $array($i) ] != "" } {
      if { $string != "" } {
	append string " "
      }
      append string $array($i)
    }
  }

  return $string
}

proc GArr_separateLinesComm { lin com commentstring { leave_comstr 0 } } {
  upvar $lin lines
  upvar $com comments

  if { [ string tolower $leave_comstr ] == "true" } {
    set leave_comstr 1
  }

  if { $leave_comstr == "1" } {
    set comlen 0
  } else {
    set comlen [ string length $commentstring ]
  }

  for { set i 0 } { [ info exists lines($i) ] } { incr i } {
    set idx [ string first $commentstring $lines($i) ]
    if { $idx >= 0 } {
      set comments($i) [ string range $lines($i) [ expr $idx + $comlen ] end ]
      set lines($i) [ string range $lines($i) 0 [ expr $idx - 1 ] ]
    } else {
      set comments($i) ""
    }
  }
}

#
# Checks, if a file is empty or only contains comments.
# Comments must start with optional commentstr and reach
# until end of line
#
proc GFile_fileEmpty { filename { commentstr "" } } {
  set failed [ GFile_readFile $filename lines num_lines ]
  if { $failed } {
    return -1
  }

  if { $commentstr != "" } {
    GArr_separateLinesComm lines comms $commentstr
  }

  if { [ string trim [ GTyCv_arrayToString lines $num_lines ] ] == "" } {
    return 1
  }

  return 0
}

#
# chain together lines in the array named as 1st arg, if
# a trailing backslash character is found. The optional
# commentsign indicates, that a line with this character
# as the first non-blank is a comment and thus ignored
#
proc GFmCv_combBackslLines { linesptr { commentsign "" } } {
  upvar $linesptr lines

  for { set i 0 } { [ info exists lines($i) ] } { incr i } {
    set l [ expr [ string length $lines($i) ] - 1 ]
    set j [ expr $i + 1 ]
    if { [ string index $lines($i) $l ] == "\\" && [ info exists lines($j) ] && [ string index [ string trim $lines($i) ] 0 ] != $commentsign } {
      set lines($i) [ string range $lines($i) 0 [ expr $l - 1 ] ]
      set lines($i) "$lines($i)$lines($j)"
      set m $i
      set n $j
      for { incr m ; incr n } { [ info exists lines($n) ] } { incr m ; incr n } {
	set lines($m) $lines($n)
      }
      unset lines($m)
      incr i -1
    }
  }

  return $i
}

#
# matches a given pattern to existing filesystem entries
# and returns two lists of their basenames (without leading
# paths): One list of directory entries, one of other entries
#
proc GFile_matchPath { path } {
  set allentries "[ glob -nocomplain $path ]"
  set dirs ""
  set files ""
  foreach entry $allentries {
    set entryt [ file tail $entry ]
    if { [ file isdirectory $entry ] } {
      if { $entryt != ".." && $entryt != "." } {
	lappend dirs $entryt
      }
    } else {
      lappend files $entryt
    }
  }

  set e ""
  lappend e $files $dirs
  return $e
}

#
# read the given directory and return two lists of entries:
# One list with subdirectories and one with other entries.
#
proc GFile_readDir { path } {
  if { ! [ file isdirectory $path ] } {
    return ""
  }

  set allentries "[ glob $path/* $path/.* ]"
  set dirs ""
  set files ""
  foreach entry $allentries {
    if { [ file isdirectory $entry ] } {
      if { $entry != ".." && $entry != "." } {
	lappend dirs [ file tail $entry ]
      }
    } else {
      lappend files [ file tail $entry ]
    }
  }

  set e ""
  lappend e $files $dirs
  return $e
}

#
# Merge together n given TCL-lists removing duplicate
# entries
#
proc GList_mergeLists { args } {
  set new_list ""

  foreach list "$args" {
    foreach elem $list {
      if { [ lsearch -exact $new_list $elem ] < 0 } {
	lappend new_list $elem
      }
    }
  }

  return $new_list
}

#
# Replace any occurrence of the 2nd argument in the 1st argument
# with the 3rd argument
#
proc GStr_replSubstring { string to_repl replacement } {
  set new ""
  set repllen [ string length $to_repl ]

  while { $string != "" } {
    set idx [ string first $to_repl $string ]
    if { $idx >= 0 } {
      append new [ string range $string 0 [ expr $idx - 1 ] ]
      append new $replacement
      set string [ string range $string [ expr $idx + $repllen ] end ]
    } else {
      append new $string
      break
    }
  }

  return $new
}

#
# replace backslash sequences of type \n or \t
# with the appropriate ASCII-characters and return
# the result
#
proc GStr_replBackslSeq { string } {
  set new ""

  while { [ set idx [ string first "\\" $string ] ] >= 0 } {
    append new [ string range $string 0 [ expr $idx - 1 ] ]
    set c [ string index $string [ expr $idx + 1 ] ]
    set string [ string range $string [ expr $idx + 2 ] end ]
    if { [ string first $c "tn" ] >= 0 } {
      switch $c {
	"t" {
	  append new "\t"
	}
	"n" {
	  append new "\n"
	}
	"r" {
	  append new "\r"
	}
	"a" {
	  append new "\a"
	}
	"b" {
	  append new "\b"
	}
      }
    } else {
      append new $c
    }
  }

  append new $string

  return $new
}

#
# Remove any garbage from a given path (e.g. //// -> / )
# and return the result
#
proc GFile_cleanPath { path } {
  set newpath ""

  while { $path != $newpath } {
    set newpath $path
    set path [ GStr_replSubstring $path "/./" "/" ]
    while { [ string first "//" $path ] >= 0 } {
      set path [ GStr_replSubstring $path "//" "/" ]
    }
    if { [ string range $path 0 1 ] == "./" } {
      set path [ string range $path 2 end ]
    }
    set l [ string length $path ]
    incr l -1
    if { [ string range $path $l $l ] == "/"  && $l > 0 } {
      set path [ string range $path 0 [ expr $l - 1 ] ]
      incr l -1
    }
    if { [ string range $path [ expr $l -1 ] $l ] == "/." } {
      if { $path == "/." } {
	set path "/"
      } else {
	set path [ string range $path 0 [ expr $l - 1 ] ]
      }
    }
    if { [ string range $path 0 3 ] == "/../" } {
      set path [ string range $path 3 end ]
    }
  }

  return $path
}

#
# Scans the file named in 1st arg and tells, if the
# line matching the regular expression in the 2nd
# argument is not commented out. Then 1 is returned,
# otherwise 0
#
proc GFile_lineActive { filename pattern { commentstr "#" } } {
  set failure [ GFile_readFile $filename lines num_lines list ]
  if { $failure } {
    return -1
  }

  set comlen [ string length $commentstr ]
  set comlast [ expr $comlen - 1 ]

  foreach line $lines {
    if { [ regexp $pattern $line ] } {
      if { [ string range [ string trim $line ] 0 $comlast ] == $commentstr } {
	return 0
      } else {
	return 1
      }
    }
  }

  return -1
}

proc GFile_commentLines { mode filename commentstr args } {
  set failure [ GFile_readFile $filename lines num_lines list ]
  if { $failure } {
    return 1
  }
  set mode [ GStr_replSubstring [ string tolower $mode ] "_" "" ]
  if { [ string range $mode 0 1 ] == "un" } {
    set newmode "de"
    append newmode [ string range $mode 2 end ]
    set mode $newmode
  }
  set comlen [ string length $commentstr ]
  set comlast [ expr $comlen - 1 ]
  set range 0
  set have_first 0
  set have_last 0
  set new_lines ""

  if { $mode == "commentrange" || $mode == "decommentrange" } {
    set first [ lindex $args 0 ]
    set last [ lindex $args 1 ]
    set range 1
  } else {
    set patterns [ lindex $args 0 ]
  }

  foreach line $lines {
    set valid_line 0

    if { $range } {
      if { $have_first } {
	set valid_line 1
	if { $have_last } {
	  set valid_line 0
	} else {
	  if { [ regexp $last $line ] } {
	    set have_last 1
	  }
	}
      } else {
	if { [ regexp $first $line ] } {
	  set valid_line 1
	  set have_first 1
	}
      }
    } else {
      foreach pat $patterns {
	if { [ regexp $pat $line ] } {
	  set valid_line 1
	  break
	}
      }
    }

    if { $valid_line } {
      set comidx [ string first $commentstr $line ]
      if { [ string range [ string trim $line ] 0 $comlast ] == $commentstr } {
	if { $mode == "decommentrange" || $mode == "decommentpatterns" } {
	  set head [ string range $line 0 [ expr $comidx - 1 ] ]
	  set tail [ string range $line [ expr $comidx + $comlen ] end ]
	  set line "$head$tail"
	}
      } else {
        if { $mode == "commentrange" || $mode == "commentpatterns" } {
	  set line "$commentstr$line"
      	}
      }
    }

    lappend new_lines $line
  }

  set failure [ GFile_writeFile $filename new_lines $num_lines list ]
  if { $failure } {
    return 1
  }

  return 0
}

proc GFile_replaceLines { filename patlist substlist } {
  set failed [ GFile_readFile $filename lines num_lines list ]
  if { $failed } {
    return $failed
  }

  set new_list [ GList_replaceElements $lines $patlist $substlist ]

  set failed [ GFile_writeFile $filename new_list [ llength $new_list ] list ]
  if { $failed } {
    return $failed
  }

  return 0
}

proc GList_replaceElements { lines patlist substlist { up_to 0 } } {
  if { $up_to == 1 || [ string tolower $up_to ] == "true" } {
    set up_to 1
  } else {
    set up_to 0
  }
  set first_pat [ lindex $patlist 0 ]
  set last_pat [ lindex $patlist 1 ]
  set have_first 0
  set have_last 0

  set new_list ""

  foreach line $lines {
    if { $have_first } {
      if { $have_last } {
	lappend new_list $line
      } else {
	if { [ regexp $last_pat $line ] } {
	  set have_last 1
	  if { $up_to } {
	    lappend new_list $line
	  }
	}
      }
    } else {
      if { [ regexp $first_pat $line ] } {
	set have_first 1
	foreach subst $substlist {
	  lappend new_list $subst
	}
	if { $last_pat == "" } {
	  set have_last 1
	}
      } else {
	lappend new_list $line
      }
    }
  }

  if { ! $have_first } {
    foreach subst $substlist {
      lappend new_list $subst
    }
  }

  return $new_list
}

proc GList_stanzas { mode lines stanzaname args } {
  set mode [ string tolower $mode ]
  set first_pat ""
  set l [ list {^[ } "\t" {]*\[[ } "\t" {]*} $stanzaname {[ } "\t" {]*\][ } "\t" {]*$} ]
  foreach p $l {
    append first_pat $p
  }
  set last_pat ""
  set l [ list {^[ } "\t" {]*\[[^]]*\][ } "\t" {]*$} ]
  foreach p $l {
    append last_pat $p
  }

  set first_line -1
  set last_line -1
  set stanza ""

  set i 0
  foreach line $lines {
    if { $first_line >= 0 } {
      if { $last_line < 0 } {
	if { [ regexp $last_pat $line ] } {
	  set last_line $i
	} else {
	  if { [ string trim $line ] != "" } {
	    lappend stanza $line
	  }
	}
      }
    } else {
      if { [ regexp $first_pat $line ] } {
	set first_line $i
      }
    }
    incr i
  }
}

#
# Find the file in the path named as 2nd argument. The
# path should be supplied as TCL-list (not colon-separated)
#
proc GFile_findFileInPathVar { file pathl } {
  upvar $pathl pathlist
  global	GVar_foundPathFiles

  if { ! [ info exists pathlist ] } {
    return $file
  }

  if { [ info exists GVar_foundPathFiles($file<$pathl) ] } {
    return $GVar_foundPathFiles($file<$pathl)
  }

  foreach path $pathlist {
    if { [ file readable $path/$file ] } {
      set GVar_foundPathFiles($file<$pathl) $path/$file
      return $path/$file
    }
  }

  return $file
}

#
# Returns the full path to the program given as argument,
# if it were tried to be started with the current PATH
# setting
#
proc GFile_findProgram { program } {
  global	env

  set program [ file tail $program ]

  set path "$env(PATH):/usr/local/bin:/usr/local/gnu:/usr/local/gnu/bin:/usr/local/bin/gnu:/opt/bin:/opt/gnu/bin:/opt/bin/gnu:/usr/bin:/bin:/usr/local/bin/X11:/opt/bin/X11:/opt/X11/bin:/usr/local/X11/bin:/usr/X11R5/bin:/usr/X11X6/bin:/usr/bsd:/usr/ucb:/usr/ccs/bin:/usr/sbin:/usr/etc:/sbin"

  set n [ GFmCv_lineToFieldsVar $path dirs : ]
  set found 0
  for { set i 0 } { $i < $n } { incr i } {
    if { [ file executable "$dirs($i)/$program" ] } {
      set found 1
      break
    }
  }

  if { $found } {
    return "$dirs($i)/$program"
  } else {
    return ""
  }
}

proc GFile_editAssignment { filename varname val mode } {
    set failed [ GFile_readFile $filename lines num_lines ]
    if { $failed } {
	GIO_errorMessage "Cannot read file $filename."
    } else {
	set found 0
	for { set actlineno 0 } { $actlineno < $num_lines } { incr actlineno } {
	  set line [ string trim $lines($actlineno) ]
	  if { [ regexp -indices "^ *$varname *= *" $line indices ] } {
	    set f [ expr [ lindex $indices 1 ] + 1 ]
	    set line [ string range $line $f end ]
	    set rest ""
	    set found 1
	    break
	  }
	}
	if { ! $found } {
	  GIO_errorMessage "Cannot find line, where $varname is set."
	} else {
	  if { [ string index $line 0 ] == "\"" } {
	    set line [ string range $line 1 end ]
	  }
	  set l [ string first "\"" $line ]
	  if { $l >= 0 } {
	    set rest [ string range $line [ expr $l + 1 ] end ]
	    set line [ string range $line 0 [ expr $l - 1 ] ]
	  }
	  if { $mode == "getval" } {
	    return $line
	  }
	  set i [ lsearch -exact $line $val ]
	  set found [ expr $i >= 0 ? 1 : 0 ]
	  if { $mode == "get" } {
	    return $found
	  }
	  set do_write 0
	  if { $found && $mode == "clear" } {
	    set line [ lreplace $line $i $i ]
	    set lines($actlineno) "$varname=\"$line\"$rest"
	    set do_write 1
	  }
	  if { ! $found && $mode == "set" } {
	    lappend line "$val"
	    set lines($actlineno) "$varname=\"$line\"$rest"
	    set do_write 1
	  }
	  if { $mode == "setval" } {
	    set lines($actlineno) "$varname=\"$val\"$rest"
	    set do_write 1
	  }
	  if { $do_write } {
	    set failed [ GFile_writeFile $filename lines $num_lines ]
	    if { $failed } {
		GIO_errorMessage "Unable to write file $lpd_startup_filename."
	    }
	  }
	}
    }
}

#
# host resolution stuff
set failed [ catch { exec ypwhich } ]
if { $failed } {
  set yp 0
} else {
  set yp 1
}

set failed [ catch { set Conf_nisDomain [ exec domainname ] } ]
if $failed {
  set Conf_nisDomain ""
}
set GCon_qualifiedNameRE {[^.]+[.][^.]+}
if { ! [ regexp $GCon_qualifiedNameRE $Conf_nisDomain ] } {
  set Conf_nisDomain ""
}

proc GHost_getHosts { } {
  global	Conf_hostsFilename yp

  if { $yp } {
    set hostsfile "| ypcat hosts"
  } else {
    set hostsfile $Conf_hostsFilename
  }
  set hosts ""
  set failed [ GFile_readFile $hostsfile hlines num_hlines ]
  if { ! $failed } {
    GArr_separateLinesComm hlines c #
    GArr_cutField hlines $num_hlines 1
    set hosts [ GTyCv_arrayToString hlines $num_hlines ]
    set idx [ lsearch -exact $hosts "localhost" ]
    if { $idx >= 0 } {
      set hosts [ lreplace $hosts $idx $idx ]
    }
  }

  return $hosts
}

proc GHost_reduceHostlistLocal { li } {
  upvar $li list
  global	Conf_nisDomain GCon_qualifiedNameRE

  if { $Conf_nisDomain == "" } {
    return
  }

  while 1 {
    set idx [ lsearch -glob $list *.$Conf_nisDomain ]
    if { $idx < 0 } {
      break
    }

    set host [ lindex $list $idx ]
    set dot [ string first . $host ]
    set host [ string range $host 0 [ expr $dot - 1 ] ]
    set hidx [ lsearch -exact $list $host ]
    if { $hidx >= 0 } {
      set list [ lreplace $list $idx $idx ]
    } else {
      set list [ lreplace $list $idx $idx $host ]
    }
  }
}

proc GHost_expandHostlistLocal { li } {
  upvar $li list
  global	Conf_nisDomain GCon_qualifiedNameRE

  if { $Conf_nisDomain == "" } {
    return
  }

  GHost_reduceHostlistLocal list

  foreach elem $list {
    if { ! [ regexp $GCon_qualifiedNameRE $elem ] } {
      lappend list "${elem}.$Conf_nisDomain"
    }
  }
}

#
# Check, if the application with the given name (must be
# the name of the current app) is already running. If yes
# and the flag exit is set (default), the program exits.
# otherwise a dialog is showed telling the user about the
# already running app
#
proc GProc_isAppRunning { name { exit 1 } } {
  set n 0

  set list [ winfo interps ]
  set l [ llength $list ]

  for { set i 0 } { $i < $l } { incr i } {
    if { $name == [ lindex [ lindex $list $i ] 0 ] } {
      incr n
      if { $n >= 2 } {
	if { $exit } {
	  frame .w
	  option add *l.wrapLength 3i widgetDefault
	  label .w.l -justify left -text "The application $name is already running. Before starting a second one of this type, please close the running one first."
	  catch { .w.l configure -font -Adobe-Times-Medium-R-Normal--*-180-*-*-*-*-*-* }
	  button .w.b -text "Ok" -command "exit 1"
	  pack .w.l .w.b -side top
	  GGUI_packAndCenter .w
	  tkwait window .
	  exit 1
	} else {
	  GGUI_warningDialog "The application $name is already running."
	  return
	}
      }
    }
  }
}

#
# source-s the named file. The global variable Conf_libraryPath
# can be set up (TCL-list) as search path for this file
#
proc GFile_loadTclSource { filename } {
  global	GVar_sourcedFiles Conf_libraryPath

  set newfilename [ GFile_findFileInPathVar $filename Conf_libraryPath ]
  if { $newfilename == $filename } {
    set newfilename [ GFile_findFileInPathVar "$filename.tcl" Conf_libraryPath ]
  }

  if { ! [ info exists GVar_sourcedFiles($newfilename) ] } {
    if { ! [ file readable $newfilename ] } {
      return 1
    }

    uplevel #0 source $newfilename
    set GVar_sourcedFiles($newfilename) 1
  }

  return 0
}

#
# If the given file is a symlink, return the path
# with the basename resolved i.e. the symlink been read
#
proc GFile_resolveLink { path } {
  set errfl [ catch { set link [ file readlink $path ] } ]

  if { $errfl } {
    return $path
  }

  set link [ GFile_cleanPath $link ]

  if { [ string range $link 0 0 ] == "/" } {
    return $link
  }

  set d [ file dirname $path ]

  return [ GFile_cleanPath "$d/$link" ]
}

#
# Resolve all occurring symlinks in the given path and
# return a path completely free of symlinks and cleaned
# from any other garbage
#
proc GFile_resolvePath { path } {
  if { $path == "/" || $path == "." } {
    return $path
  }

  set link [ GFile_resolveLink $path ]
  if { $link != $path } {
    set link [ GFile_resolvePath $link ]
    if { $link == "" } {
      return ""
    }
  }

  set p [ GFile_resolvePath [ file dirname $link ] ]
  set t [ file tail $link ]

  if { ! [ file exists "$p/$t" ] } {
    return ""
  }

  return [ GFile_cleanPath "$p/$t" ]
}

#
# Resolve all occurring symlinks in the given path and
# return a path completely free of symlinks and cleaned
# from any other garbage. Furthermore remove upward
# references (e.g. /usr/bin/../lib -> /usr/lib)
#
proc GFile_resolvePath__ { path } {
  set path [ GFile_resolvePath $path ]

  set prefix ""
  while { [ string range $path 0 2 ] == "../" } {
    append prefix "../"
    set path [ string range $path 3 end ]
  }

  set newpath ""
  while { $newpath != $path } {
    set newpath $path

    regsub {[^/]+/[.][.]/} $path "" path
  }

  set newpath ""
  while { $newpath != $path } {
    set newpath $path
    regsub {[^/]+/[.][.]$} $path "" path
    if { $path == "" } {
      set path "."
    }
  }

  append prefix [ GFile_cleanPath $path ]

  return $prefix
}

proc GGUI_getActPos { } {
  global	GVar_actMenuPath GVar_actXPos GVar_actYPos
  global	GVar_parentFrame GVar_menuItems

  if { [ info exists GVar_actMenuPath ] } {
    set achild [ lindex $GVar_menuItems($GVar_actMenuPath) 0 ]
    set failed [ catch { set geom [ wm geometry $GVar_parentFrame($achild) ] } ]
    if { ! $failed } {
      scan $geom "%dx%d%d%d" w h x y
      return "$x $y"
    }
  }
  if { [ info exists GVar_actXPos ] && [ info exists GVar_actYPos ] } {
    return "$GVar_actXPos $GVar_actYPos"
  }

  return ""
}

proc GGUI_insertInMenuTree { name } {
  global	GVar_parentMenus GVar_menuItems

      if { [ string index $name 0 ] == "/" } {
	set names $name
      } else {
	set names ""

	set lname $name
	set rest [ file tail $name ]
	while 1 {
	  if { [ info exists GVar_parentMenus($lname) ] } {
	    foreach m $GVar_parentMenus($lname) {
	      if { $m == "/" } {
		set m ""
	      }
	      if { [ lsearch -exact $names "$m/$rest" ] < 0 } {
		lappend names "$m/$rest"
	      }
	    }
	    break
	  }

	  set idx [ string last / $lname ]
	  if { $idx < 0 } {
	    puts stderr "Warning: Menu item $name not yet known, ignored."
	    set names ""
	    break
	  }

	  set lname [ string range $lname 0 [ expr $idx - 1 ] ]
	  set resthead [ file tail $lname ]
	  set rest "$resthead/$rest"
	}
      }
      foreach name $names {
	while { $name != "/" && $name != "" } {
	  set parent [ file dirname $name ]

	  if { ! [ info exists GVar_menuItems($parent) ] } {
	    set GVar_menuItems($parent) $name
	  } else {
	    if { [ lsearch -exact $GVar_menuItems($parent) $name ] < 0 } {
	      lappend GVar_menuItems($parent) $name
	    } else {
	      break
	    }
	  }

	  if { ! [ info exists GVar_parentMenus($name) ] } {
	    set GVar_parentMenus($name) $parent
	  } else {
	    if { [ lsearch -exact $GVar_parentMenus($name) $parent ] < 0 } {
	      lappend GVar_parentMenus($name) $parent
	    }
	  }
	  set lname [ string range $name 1 end ]
	  while { $lname != "" } {
	    if { ! [ info exists GVar_parentMenus($lname) ] } {
	      set GVar_parentMenus($lname) $parent
	    } else {
	      if { [ lsearch -exact $GVar_parentMenus($lname) $parent ] < 0 } {
		lappend GVar_parentMenus($lname) $parent
	      }
	    }
	    set idx [ string first / $lname ]
	    if { $idx >= 0 } {
	      set lname [ string range $lname [ expr $idx + 1 ] end ]
	    } else { 
	      break
	      set lname ""
	    }
	  }
	  set name $parent
	}
      }

  return $names
}

proc GGUI_insertExistingItemsRecursive { menu itempath } {
  global	GVar_menuItemLabel GVar_menuItemProc
  global	GVar_menuItemFilename GVar_menuItemCondition
  global	GVar_menuLabel GVar_menuTitle GVar_menuItems

  GGUI_insertInMenuTree "$menu"
  foreach vars "GVar_menuItemLabel GVar_menuItemProc GVar_menuItemFilename GVar_menuItemCondition GVar_menuLabel GVar_menuTitle" {
    if { [ info exists "${vars}($itempath)" ] } {
      set "${vars}($menu)" [ set "${vars}($itempath)" ]
    }
  }

  if { [ info exists GVar_menuItems($itempath) ] } {
    foreach menuitem $GVar_menuItems($itempath) {
      set itemname [ file tail $menuitem ]
      GGUI_insertExistingItemsRecursive "$menu/$itemname" "$itempath/$itemname"
    }
  }
}

proc GGUI_readMenuConfiguration { filename { option "" } } {
  global	GVar_parentMenus GVar_menuItems GVar_sourcedFiles
  global	GVar_menuItemLabel GVar_menuItemProc
  global	GVar_menuItemFilename GVar_menuItemCondition
  global	GVar_subframeExists GVar_menuLabel GVar_menuTitle

  set menuitemcmd "menuitem"
  set menuhookcmd "menuhook"
  set itemlabelcmd "itemlabel"
  set itemproccmd "itemproc"
  set conditioncmd "condition"
  set menulabelcmd "menulabel"
  set menutitlecmd "menutitle"
  set validcmds "$menuitemcmd $itemlabelcmd $itemproccmd $conditioncmd $menulabelcmd $menutitlecmd $menuhookcmd"

  set menu_conf [ GFile_readFilesAsListRecursive $filename ]

  if { [ string tolower [ string range $option 0 3 ] ] != "keep" } {
    foreach var { GVar_parentMenus GVar_menuItems GVar_menuItemProc
		GVar_menuItemCondition GVar_menuItemFilename
		GVar_sourcedFiles GVar_menuItemLabel
		GVar_subframeExists } {
      catch { unset $var }
    }
  }

  foreach line $menu_conf {
    set cmd [ string tolower [ lindex $line 0 ] ]

    if { [ lsearch -exact $validcmds $cmd ] >= 0 } {
      set names [ GGUI_insertInMenuTree [ lindex $line 1 ] ]
    }

    switch $cmd \
      "" { } \
      $menuitemcmd { } \
      $itemlabelcmd {
	set itemlabeltype [ string tolower [ lindex $line 2 ] ]
	set label [ lindex $line 3 ]
	foreach name $names {
	  switch $itemlabeltype {
	    "var" {
	      set GVar_menuItemLabel($name) {$}
	    }
	    "string" {
	      set GVar_menuItemLabel($name) {"}
	      set label [ GStr_replBackslSeq $label ]
	    }
	  }
	  append GVar_menuItemLabel($name) $label
	}
      } \
      $itemproccmd {
	set proc [ lindex $line 2 ]
	set file [ lindex $line 3 ]

	foreach name $names {
	  set GVar_menuItemFilename($name) $file
	  set GVar_menuItemProc($name) $proc
	}
      } \
      $conditioncmd {
	set condtype [ string tolower [ lindex $line 2 ] ]
	set condition [ lindex $line 3 ]
	foreach name $names {
	  switch $condtype {
	    "var" {
	      set GVar_menuItemCondition($name) {$}
	    }
	    "expr" {
	      set GVar_menuItemCondition($name) {@}
	    }
	    "file" {
	      set GVar_menuItemCondition($name) {/}
	    }
	  }
	  append GVar_menuItemCondition($name) [ string trim $condition ]
	}
      } \
      $menulabelcmd {
	set menulabeltype [ string tolower [ lindex $line 2 ] ]
	set label [ lindex $line 3 ]
	foreach name $names {
	  switch $menulabeltype {
	    "var" {
	      set GVar_menuLabel($name) {$}
	    }
	    "string" {
	      set GVar_menuLabel($name) {"}
	      set label [ GStr_replBackslSeq $label ]
	    }
	  }
	  append GVar_menuLabel($name) $label
	}
      } \
      $menutitlecmd {
	set menutitletype [ string tolower [ lindex $line 2 ] ]
	set title [ lindex $line 3 ]
	foreach name $names {
	  switch $menutitletype {
	    "var" {
	      set GVar_menuTitle($name) {$}
	    }
	    "string" {
	      set GVar_menuTitle($name) {"}
	      set title [ GStr_replBackslSeq $title ]
	    }
	  }
	  append GVar_menuTitle($name) $title
	}
      } \
      $menuhookcmd {
	set insert_points [ GGUI_insertInMenuTree [ lindex $line 2 ] ]
	foreach name $names {
	  foreach insert_point $insert_points {
	    GGUI_insertExistingItemsRecursive $insert_point $name
	  }
	}
      } \
      default {
	if { [ string index [ string trim $line ] 0 ] != "#" } {
	  puts stderr "Warning: illegal configuration line:\n\"$line\""
	}
      }
    \
  }

  set GVar_parentMenus(/) _
}

proc GGUI_selectionProc { { menu "/" } } {
  global	GVar_parentMenus GVar_menuItems GVar_sourcedFiles
  global	GVar_actMenuPath GVar_menuItemLabel GVar_actWidget
  global	GVar_menuItemProc GVar_menuItemFilename
  global	GVar_menuItemCondition GVar_actMenuPath
  global	GVar_parentFrame Conf_menuXDist Conf_libraryPath
  global	Conf_menuYDist GVar_actXPos GVar_actYPos GVar_subframeExists
  global	GVar_menuTreeDialogShowed GVar_menuLabel GVar_menuTitle

  set top_widget .app

  if { [ info exists GVar_menuItemProc($menu) ] } {
    set proc [ string trim $GVar_menuItemProc($menu) ]
    if { $proc != "" } {
      set can_execute 1
      set program [ lindex $proc 0 ]

      if { [ info proc $program ] == "" && [ info commands $program ] == "" } {
	set filename $GVar_menuItemFilename($menu)
	if { [ GFile_loadTclSource $filename ] } {
	  GIO_errorMessage "Cannot read file \"$filename\" for executing procedure \"$proc\"."
	  set can_execute 0
	}
	if { [ info proc $program ] == "" } {
	  GIO_errorMessage "Cannot find procedure \"$program\" in file \"$filename\"."
	  set can_execute 0
	}
      }

      if { $can_execute } {
	eval $proc
      }
    }
  }


  if { ! [ info exists GVar_menuItems($menu) ] } {
    return
  }

  set parentmenu [ file dirname $menu ]
  if { $menu == "/" } {
    set parentmenu _
  }

  if { [ llength $GVar_parentMenus($menu) ] > 1 } {
    set idx [ lsearch -exact $GVar_parentMenus($menu) $parentmenu ]
    if { $idx < 0 } {
      GIO_errorMessage "Internal error: $parentmenu not in array for $menu."
    } else {
      set GVar_parentMenus($menu) [ linsert [ lreplace $GVar_parentMenus($menu) $idx $idx ] 0 $parentmenu ]
    }
  } else {
    if { $parentmenu != $GVar_parentMenus($menu) } {
      GIO_errorMessage "Internal error: $parentmenu not in array for $menu."
    }
    set parentmenu $GVar_parentMenus($menu)
  }

  set parentmenu [ lindex $GVar_parentMenus($menu) 0 ]
  if { [ info exists GVar_subframeExists($parentmenu) ] } {
    bell
    if { ! [ info exists GVar_menuTreeDialogShowed ] } {
      set GVar_menuTreeDialogShowed 1
      GGUI_genericDialog .info "Information" "For keeping your screen clearly arranged and the system configuration files consistent you are not allowed to open more than one subtree at the same time. Nonetheless you can close a total subtree by pressing Cancel in any dialog. (This message won't be popped up again until exit)" "" 0 Ok
    }
    return
  }

  set GVar_subframeExists($parentmenu) 1

  set widget [ GStr_replSubstring $menu / . ]
  if { $widget == "." } {
    set widget $top_widget
  } else {
    set widget "$top_widget$widget"
  }

  set GVar_actMenuPath $menu
  set GVar_actWidget $widget

  catch { destroy $widget }

  if { $widget == "$top_widget" } {
    frame $widget
    set actparentframe [ winfo parent $widget ]
  } else {
    toplevel $widget
    set actparentframe $widget

    set getpos_failed [ catch { set geometry [ wm geometry $GVar_parentFrame($menu) ] } ]
    if { ! $getpos_failed } {
      scan $geometry "%dx%d%d%d" w h GVar_actXPos GVar_actYPos

      incr GVar_actXPos $Conf_menuXDist
      incr GVar_actYPos $Conf_menuYDist
    }
  }

  if { [ info exists GVar_menuLabel($menu) ] } {
    set type [ string index $GVar_menuLabel($menu) 0 ]
    set body [ string range $GVar_menuLabel($menu) 1 end ]
    switch $type {
      {$} {
	global $body
	if { [ info exists $body ] } {
	  set text [ set $body ]
	} else {
	  set text ""
	}
      }
      {"} {
	set text $body
      }
    }
    label "$widget.__GVar_menuLabel__" -text $text
    pack "$widget.__GVar_menuLabel__" -in $widget -side top
  }

  set childwidget $widget.__dummy__

  foreach child $GVar_menuItems($menu) {
    set amenuitem $child
    set GVar_parentFrame($child) $actparentframe

    if { [ info exists GVar_menuItemCondition($child) ] } {
      set type [ string index $GVar_menuItemCondition($child) 0 ]
      set body [ string range $GVar_menuItemCondition($child) 1 end ]
      switch $type {
	{$} {
	  global $body
	  set ok 0
	  if { [ info exists $body ] } {
	    set val [ set $body ]
	    if { $val == "1" || [ string tolower $val ] == "true" } {
	      set ok 1
	    }
	  }
	  if { ! $ok } {
	    continue
	  }
	}
	"/" {
	  if { ! [ file exists $body ] } {
	    continue
	  }
	}
	"@" {
	  if { ! [ eval $body ] } {
	    continue
	  }
	}
      }
    }

    set childhead [ file dirname $child ]
    if { $childhead == "/" } {
      set childhead ""
    }
    set childtail [ file tail $child ]
    set childwidget [ GStr_replSubstring "$childhead/__${childtail}__" / . ]
    set childwidget "$top_widget$childwidget"

    frame $childwidget
    button $childwidget.button -text "" -command "GGUI_selectionProc $child"
    if { [ info exists GVar_menuItemLabel($child) ] } {
      set body [ string range $GVar_menuItemLabel($child) 1 end ]
      switch [ string index $GVar_menuItemLabel($child) 0 ] {
	{"} {
	  set label $body
	}
	{$} {
	  global $body
	  set label [ set $body ]
	}
      }
    } else {
      set label [ file tail $child ]
    }
    label $childwidget.label -text $label

    pack $childwidget.button $childwidget.label -side left
    pack $childwidget -in $widget -side top -anchor w
  }

  if { [ info exists GVar_menuTitle($menu) ] } {
    set type [ string index $GVar_menuTitle($menu) 0 ]
    set body [ string range $GVar_menuTitle($menu) 1 end ]

    switch $type {
      {$} {
	global $body
	if { [ info exists $body ] } {
	  set title [ set $body ]
	} else {
	  set title ""
	}
      }
      {"} {
	set title $body
      }
    }
  } else {
    if { [ info exists GVar_menuItemLabel($menu) ] } {
      set body [ string range $GVar_menuItemLabel($menu) 1 end ]
      switch [ string index $GVar_menuItemLabel($menu) 0 ] {
	{"} {
	  set title $body
	}
	{$} {
	  global $body
	  set title [ set $body ]
	}
      }
    } else {
      set title [ file tail $menu ]
    }
  }

  if { $widget == "$top_widget" } {
    frame "${childwidget}_can_"
    button "${childwidget}_can_.cancel" -text "Exit" -command "exit 0"

    pack "${childwidget}_can_.cancel"
    pack "${childwidget}_can_" -in $widget -side top

    pack $widget
    catch { wm geometry [ winfo parent $widget ] "+$GVar_actXPos+$GVar_actYPos" }
    catch { wm title [ winfo parent $widget ] $title }
  } else {
    frame "${childwidget}_can_"
    button "${childwidget}_can_.cancel" -text "Cancel" -command "GGUI_destroyFrametree $widget $menu"

    pack "${childwidget}_can_.cancel"
    pack "${childwidget}_can_" -in $widget -side top

    if { ! $getpos_failed } {
      catch { wm geometry $GVar_parentFrame($amenuitem) "+$GVar_actXPos+$GVar_actYPos" }
    }
    wm protocol $GVar_parentFrame($amenuitem) WM_DELETE_WINDOW "GGUI_destroyFrametree $widget $menu"
    catch { wm title $GVar_parentFrame($amenuitem) $title }
  }
}

proc GGUI_destroyFrametree { widget menu } {
  global	GVar_subframeExists GVar_parentMenus
  global	GVar_parentFrame GVar_actMenuPath GVar_actWidget

  destroy $widget

  set parentmenu [ lindex $GVar_parentMenus($menu) 0 ]
  foreach frame [ array names GVar_subframeExists ] {
    if { [ string length $parentmenu ] <= [ string length $frame ] } {
      catch { unset GVar_subframeExists($frame) }
    }
  }

  set GVar_actWidget $GVar_parentFrame($menu)
  set GVar_actMenuPath [ lindex $GVar_parentMenus($menu) 0 ]
}

proc GGUI_redisplayActMenu { } {
  global	GVar_actMenuPath GVar_subframeExists GVar_parentMenus

  set parentmenu [ lindex $GVar_parentMenus($GVar_actMenuPath) 0 ]
  catch { unset GVar_subframeExists($parentmenu) }

  GGUI_selectionProc $GVar_actMenuPath
}

proc GGUI_destroyActFrameTree { } {
  global	GVar_actMenuPath GVar_actWidget

  GGUI_destroyFrametree $GVar_actWidget $GVar_actMenuPath
}

proc GGUI_placeToplevel { w { change_actpos 0 } } {
  global	Conf_menuXDist Conf_menuYDist GVar_actXPos GVar_actYPos

  set pos [ GGUI_getActPos ]

  if { $pos == "" } {
    set x [expr [winfo screenwidth $w]/2 - [winfo reqwidth $w]/2 \
            - [winfo vrootx $w]]
    set y [expr [winfo screenheight $w]/2 - [winfo reqheight $w]/2 \
            - [winfo vrooty $w]]
  } else {
    set x [ lindex $pos 0 ]
    set y [ lindex $pos 1 ]
    if { [ info exists Conf_menuXDist ] } {
      incr x $Conf_menuXDist
    }
    if { [ info exists Conf_menuYDist ] } {
      incr y $Conf_menuYDist
    }
  }

  if { $change_actpos == "1" || [ string tolower $change_actpos] == "true" } {
    set GVar_actXPos $x
    set GVar_actYPos $y
  }

  wm geom $w "+$x+$y"
}

#
# pack a toplevel window and put it into the center of the screen
#
proc GGUI_packAndCenter { win } {
  pack $win
  set w [ winfo parent $win ]
  wm withdraw $w
  update idletasks

  set x [expr [winfo screenwidth $w]/2 - [winfo reqwidth $w]/2 ]
  set y [expr [winfo screenheight $w]/2 - [winfo reqheight $w]/2 ]
  wm geom $w +$x+$y
  wm deiconify .
}

proc GGUI_getPosAndDestroy { widget } {
  global GVar_actXPos GVar_actYPos

  set fault [ catch { set parent [ winfo parent $widget ] } ]
  if { $fault } {
    return
  }
  if { $parent == "" } {
    set parent "."
  }
  set fault [ catch { set winfo [ winfo geometry $parent ] } ]

  if { $fault } {
    return
  }

  scan $winfo "%dx%d%d%d" w h GVar_actXPos GVar_actYPos
  catch { destroy $widget }
}

proc GGUI_packAndPlace { widget } {
  global GVar_actXPos GVar_actYPos

  pack $widget

  set fault [ catch { set widget [ winfo parent $widget ] } ]
  if { $fault } {
    return
  }
  if { $widget == "" } {
    set widget "."
  }

  wm geometry $widget "+$GVar_actXPos+$GVar_actYPos"
}

#
# Pop up an error dialog prompting the user with the message in msg
#
proc GGUI_errorDialog { msg } {
  GGUI_genericDialog .error "Error" "Error:\n\n$msg" "" 0 Ok
}

#
# Pop up a warning dialog prompting the user with the message in msg
#
proc GGUI_warningDialog { msg } {
  GGUI_genericDialog .warning "Warning" "Warning:\n\n$msg" "" 0 Ok
}

#
# Pop up a toplevel dialog and call a procedure to build it's
# subwidgets. The procedure to call is the second argument. That
# procedure gets the parent widget and a tkPriv array element name
# as next to last and last argument. The tkPriv array element
# must be set, when the dialog should disappear. The second
# argument to may be combined of several words. The second word etc.
# is passed to the subwidgets_proc as first arguments.
#
proc GGUI_basicDialog { title subwidgets_proc } {
  global	tkPriv

  set w .dialog[clock seconds]
  catch { destroy $w }

  set tkw tkprivvar[clock seconds]
  set tkPriv($tkw) 0

  toplevel $w -class Dialog
  wm title $w $title
  wm protocol $w WM_DELETE_WINDOW "destroy $w ; set tkPriv($tkw) -1"
  wm iconname $w $title

  frame $w.f

  set ret [ eval $subwidgets_proc $w.f $tkw ]

  pack $w.f -fill both -expand 1

  tkwait variable tkPriv($tkw)

  destroy $w

  return $ret
}

#
# Pop up an information dialog prompting the user with the text
#
proc GGUI_infoDialog { text { title "Information" } } {
  global	Conf_menuXDist Conf_menuYDist
  global	GVar_actXPos GVar_actYPos

  set w .info[clock seconds]
  catch { destroy $w }

  toplevel $w -class Dialog
  wm title $w $title
  wm protocol $w WM_DELETE_WINDOW "destroy $w"
  wm iconname $w $title
#  wm transient $w [ winfo toplevel [ winfo parent $w ] ]
  frame $w.
  option add *l.wrapLength 4i widgetDefault
  label $w.l -justify left -text "$text"
  catch { $w.l configure -font -Adobe-Times-Medium-R-Normal--*-180-*-*-*-*-*-* }
#  button $w.b -text "Ok" -command "destroy $w"
  pack $w.l -side top
  GGUI_placeToplevel $w True

  set exists 1
  foreach var "Conf_menuXDist Conf_menuYDist GVar_actXPos GVar_actYPos" {
    if { ! [ info exists $var ] } {
      set exists 0
    }
  }

  update idletasks
}

#
# Internal routine for sorting
proc SDIC_compareListElements { arg1 arg2 } {
  if { [ lindex $arg1 0 ] > [ lindex $arg2 0 ] } {
    return 1
  }
  if { [ lindex $arg1 0 ] < [ lindex $arg2 0 ] } {
    return -1
  }
  return 0
}

#
# Internal callback
proc SDIC_selectAll { } {
  .seldialog.list.listbox selection set 0 end
  .seldialog.sel.all configure -text "Deselect all" -command SDIC_deselectAll
}

#
# Internal callback
proc SDIC_deselectAll { } {
  .seldialog.list.listbox selection clear 0 end
  .seldialog.sel.all configure -text "Select all" -command SDIC_selectAll
}

#
# Display a dialog to make the user select one or more items
# from the list passed in the 1st arg. The list must contain
# elements each being a combination of the item text to display
# and the value to be returned, e.g.:
# GGUI_selectionDialog { { one 1 } { two 2 } { three 3 } }
# It is that complicated, cause the displayed list will be sorted
# and so indices can be returned or whatever ...
# an empty string is returned, if nothing has been selected
#
proc GGUI_selectionDialog { list { title "Selection" } } {
  global tkPriv GVar_sdicSelectionEntry GVar_sdicSelectionList

  set sdic_num_selection_items [ llength $list ]
  set sorted_list [ lsort -command SDIC_compareListElements $list ]
  set GVar_sdicSelectionList $sorted_list
  set tkPriv(sel_marker) -2

  set w .seldialog

  catch { destroy $w }
  toplevel $w -class Dialog
  wm title $w $title
  wm protocol $w WM_DELETE_WINDOW "set tkPriv(sel_marker) -3"
  wm iconname $w $title
#  wm transient $w [ winfo toplevel [ winfo parent $w ] ]
  frame .seldialog.list
  frame .seldialog.sel
  frame .seldialog.cmds
  set scrollb 0
  set height $sdic_num_selection_items
  if { $height > 20 } {
    set height 20
    set scrollb 1
  }
  if { $scrollb } {
    listbox .seldialog.list.listbox -height $height -selectmode multiple -yscrollcommand ".seldialog.list.scrollbar set"
    scrollbar .seldialog.list.scrollbar -command ".seldialog.list.listbox yview"
  } else {
    listbox .seldialog.list.listbox -height $height -selectmode multiple
  }
  button .seldialog.sel.all -text "Select all" -command SDIC_selectAll
  button .seldialog.cmds.cancel -text Cancel -command "set tkPriv(sel_marker) -1"
  button .seldialog.cmds.done -text Done -command "set tkPriv(sel_marker) 0"

  if { $scrollb } {
    pack .seldialog.list.listbox .seldialog.list.scrollbar -side left -fill both
    pack .seldialog.list.listbox -expand 1
  } else {
    pack .seldialog.list.listbox -fill both
    pack .seldialog.list.listbox -expand 1
  }
  foreach elem $sorted_list {
    .seldialog.list.listbox insert end [ lindex $elem 0 ]
  }
  pack .seldialog.sel.all -expand 1
  pack .seldialog.cmds.done .seldialog.cmds.cancel -side left -expand 1
  pack .seldialog.list .seldialog.sel .seldialog.cmds -side top -fill both

  wm withdraw $w
  GGUI_placeToplevel $w True
  update idletasks
  wm deiconify $w 
  set old_focus [focus]
  set old_grab [ grab current $w]
  if {$old_grab != ""} {
        set grab_status [grab status $old_grab]
  }
  grab $w
  tkwait variable tkPriv(sel_marker)

  if {$old_grab != ""} {
    if {$grabStatus == "global"} {
      grab -global $old_grab
    } else {
      grab $old_grab
    }
  }

  set sel $tkPriv(sel_marker)
  set selection ""
  if { $sel >= 0 } {
    set sels [ .seldialog.list.listbox curselection ]
    if { $sels != "" } {
      foreach i "$sels" {
	lappend selection [ lindex [ lindex $sorted_list $i ] 1 ]
      }
    }
  } else {
    set selection -1
  }

  destroy $w

  return $selection
}

#
# internal callback
proc CDIC_isItemInList { item list } {
  set l [ llength $list ]

  for { set i 0 } { $i < $l } { incr i } {
    if { [ lindex [ lindex $list $i ] 0 ] == $item } {
      return $i
    }
  }

  return -1
}

#
# internal callback
proc CDIC_showInsPos { list elem widget } {
  global Conf_listHeight

  set l [ llength $list ]
  set p [ expr ($l + 1) / 2 ]
  set d [ expr ($p + 1) / 2 ]

  while { $d > 0 } {
    if { [ lindex $list $p ] > $elem } {
	set p [ expr $p - $d ]
	if { $p < 0 } {
	  set p 0
	}
    } else {
	set p [ expr $p + $d ]
	if { $p >= $l } {
	  set p [ expr $l - 1]
	}
    }

    if { $d <= 1 } {
	break
    }
    set d [ expr ($d + 1) / 2 ]
  }

  set p [ expr $p - ($Conf_listHeight / 2) ]
  if { $p < 0 } {
    set p 0
  }
  $widget yview $p
}

#
# internal callback
proc CDIC_completeSelection { item list widget } {
  set l [ llength $list ]
  set sl [ string length $item ]
  set retitem $item

  incr sl -1
  for { set i 0 } { $i < $l } { incr i } {
    set element [ lindex [ lindex $list $i ] 0 ]
    if { [ string range $element 0 $sl] == $item } {
      set retitem $element
      break
    }
  }

  CDIC_showInsPos $list $item $widget

  return $retitem
}

#
# internal callback
proc CDIC_helpOnChoiceDialog { } {
  GGUI_genericDialog .help "Help on: Selection entry" "This field allows you to enter an item of the list above. When you hit <Return>, the approprite choice is made, if your input fits an item exactly. Otherwise nothing happens. When you press <Ctrl>-<space>, your input will be completed to the first matching item in the list. If there is none matching, the list is adjusted to the position of the list, where your input could be found, if it existed." "" 0 Ok
}

#
# Display a dialog to make the user select an item
# from the list passed in the 1st arg. The list must contain
# elements each being a combination of the item text to display
# and the value to be returned, e.g.:
# GGUI_selectionDialog { { one 1 } { two 2 } { three 3 } }
# It is that complicated, cause the displayed list will be sorted
# and so an index can be returned or whatever ...
# -1 is returned, if nothing has been selected i.e. Cancel has
# been pressed. The second arg is for the prompt to be displayed
#
proc GGUI_choiceDialog { list prompt { title "Choice" } } {
  global tkPriv GVar_cdicSelectionEntry GVar_cdicNumSelectionItems
  global GVar_cdicSelectionList Conf_listHeight

  set GVar_cdicNumSelectionItems [ llength $list ]
  if { $GVar_cdicNumSelectionItems == 0 } {
    return -5
  }
  if { $GVar_cdicNumSelectionItems == 1 } {
    return [ lindex [ lindex $list 0 ] 1 ]
  }
  set sorted_list [ lsort -command SDIC_compareListElements $list ]
  set GVar_cdicSelectionList $sorted_list
  set tkPriv(sel_marker) -2

  set w .seldialog

  catch { destroy $w }
  toplevel $w -class Dialog
  wm title $w $title
  wm protocol $w WM_DELETE_WINDOW "set tkPriv(sel_marker) -3"
  wm iconname $w $title
#  wm transient $w [ winfo toplevel [ winfo parent $w ] ]
  frame .seldialog.list
  frame .seldialog.entry
  frame .seldialog.cmds
  set scrollb 0
  set height $GVar_cdicNumSelectionItems
  if { $height > $Conf_listHeight } {
    set height $Conf_listHeight
    set scrollb 1
  }
  if { $scrollb } {
    listbox .seldialog.list.listbox -height $height -selectmode single -yscrollcommand ".seldialog.list.scrollbar set"
    scrollbar .seldialog.list.scrollbar -command ".seldialog.list.listbox yview"
  } else {
    listbox .seldialog.list.listbox -height $height -selectmode single
  }
  label .seldialog.entry.label -text $prompt
  entry .seldialog.entry.entry -width 15
  bind .seldialog.entry.entry <Return> { set val [ .seldialog.entry.entry get ] ; set sel [ CDIC_isItemInList $val $GVar_cdicSelectionList ] ; if { $sel >= 0 } { set tkPriv(sel_marker) $sel }}
  bind .seldialog.entry.entry <F1> CDIC_helpOnChoiceDialog
  bind .seldialog.entry.entry <Control-space> { set val [ .seldialog.entry.entry get ] ; .seldialog.entry.entry delete 0 end ; .seldialog.entry.entry insert 0 [ CDIC_completeSelection $val $GVar_cdicSelectionList .seldialog.list.listbox ] }
  button .seldialog.cmds.cancel -text Cancel -command "set tkPriv(sel_marker) -1"

  bind .seldialog.list.listbox <ButtonPress-1> { set tkPriv(sel_marker) sel_to_get }
  if { $scrollb } {
    pack .seldialog.list.listbox .seldialog.list.scrollbar -side left -fill both
    pack .seldialog.list.listbox -expand 1
  } else {
    pack .seldialog.list.listbox -fill both
    pack .seldialog.list.listbox -expand 1
  }
  foreach elem $sorted_list {
    .seldialog.list.listbox insert end [ lindex $elem 0 ]
  }
  pack .seldialog.entry.label .seldialog.entry.entry -side left -fill x
  pack .seldialog.entry.entry -expand 1
  pack .seldialog.cmds.cancel
  pack .seldialog.list .seldialog.entry .seldialog.cmds -side top -fill both

  wm withdraw $w
  GGUI_placeToplevel $w True
  update idletasks
  wm deiconify $w 
  set old_focus [focus]
  set old_grab [ grab current $w]
  if {$old_grab != ""} {
        set grab_status [grab status $old_grab]
  }
  grab $w
  tkwait variable tkPriv(sel_marker)

  if {$old_grab != ""} {
    if {$grabStatus == "global"} {
      grab -global $old_grab
    } else {
      grab $old_grab
    }
  }

  set sel $tkPriv(sel_marker)
  if { $sel == "sel_to_get" } {
    set sel [ .seldialog.list.listbox curselection ]
    if { $sel == "" } {
      set sel -4
    }
  }

  destroy $w

  if { $sel < 0 } {
    return $sel
  }

  return [ lindex [ lindex $sorted_list $sel ] 1 ]
}

#
# help stuff
proc LDIC_helpOnListDialog { name { extended 0 } } {
  set text "This field allows you to enter an element to be added to the list above. Pressing the \n\"^ Add ^\"-button or hitting the Return-key performs the add-function. Selecting elements from the list and pressing the Remove-button removes the selected elements."
  if $extended {
    set text "This field allows you to enter an element to be added to the list on the left side above. Pressing the \n\"^ Add ^\"-button or hitting the Return-key performs the add-function. Selecting elements from the list and pressing the Remove-button removes the selected elements. You can also select elements from the list on the right side and then press \n\"< Add <\", what will copy your selection to the left side. Pressing \"Select all\" selects all elements in the right list."
  }

  GGUI_genericDialog .help "Help on: $name entry" $text "" 0 Ok
}

#
# internal callback
proc LDIC_selectAll { } {
  .seldialog.list.olistbox selection set 0 end
  .seldialog.moves.selall configure -text "Deselect all" -command LDIC_deselectAll
}

#
# internal callback
proc LDIC_deselectAll { } {
  .seldialog.list.olistbox selection clear 0 end
  .seldialog.moves.selall configure -text " Select all  " -command LDIC_selectAll
}

#
# internal callback
proc LDIC_addEntry { newname } {
  set newname [ string trim $newname ]
  set list [ .seldialog.list.listbox get 0 end ]
  if { $newname == "" } return
  set exists 0
  if { [ lsearch -exact $list $newname ] >= 0 } {
    set exists 1
  }
  if { [ llength $newname ] > 1 } {
    GIO_errorMessage "The entry must be a single word"
    return
  }
  set i 0
  foreach sel $list {
    if { $newname < $sel } break
    incr i
  }
  .seldialog.entry.entry delete 0 end
  .seldialog.list.listbox selection clear 0 end
  if { $exists } {
    return
  }
  .seldialog.list.listbox insert $i $newname
}

#
# Prompts the user with a dialog with 1st arg as title, 2nd arg
# as prompt. The 3rd arg is an optional list, that represents
# a preset selection. 4th arg is an optional list containing
# more possible selections offered in an extra list. More items
# to be selected can be typed in. Return value is the list of
# selected items or -- if cancel button pressed or window destroyed
#
proc GGUI_listDialog { title prompt { list "" } { offered_list "" } } {
  global tkPriv

  set sdic_num_selection_items [ llength $list ]
  set sorted_list [ lsort $list ]
  set GVar_sdicSelectionList $sorted_list
  set tkPriv(sel_marker) -2
  if { $offered_list == "" } {
    set extended 0
  } else {
    set extended 1
  }

  set w .seldialog

  catch { destroy $w }
  toplevel $w -class Dialog
  wm title $w $title
  wm protocol $w WM_DELETE_WINDOW "set tkPriv(sel_marker) -3"
  wm iconname $w $title
#  wm transient $w [ winfo toplevel [ winfo parent $w ] ]
  frame .seldialog.labels
  frame .seldialog.list
  frame .seldialog.moves
  frame .seldialog.entry
  frame .seldialog.cmds
  set scrollb 0
  set height 20
  listbox .seldialog.list.listbox -height $height -selectmode multiple -yscrollcommand ".seldialog.list.scrollbar set"
  scrollbar .seldialog.list.scrollbar -command ".seldialog.list.listbox yview"
  button .seldialog.moves.add -text "^  Add  ^" -command { LDIC_addEntry [ .seldialog.entry.entry get ] }
  button .seldialog.moves.remove -text Remove -command {
			set list [ .seldialog.list.listbox curselection ]
			set l [ expr [ llength $list ] - 1 ]
			for { set i $l } { $i >= 0 } { incr i -1 } {
				set sel [ lindex $list $i ]
				.seldialog.list.listbox delete $sel
			}
			.seldialog.list.listbox selection clear 0 end }
  if $extended {
    set offered_list [ lsort $offered_list ]
    label .seldialog.labels.sel -text "Selected items"
    label .seldialog.labels.off -text "Some possible items"
    listbox .seldialog.list.olistbox -height $height -selectmode multiple -yscrollcommand ".seldialog.list.oscrollbar set"
    scrollbar .seldialog.list.oscrollbar -command ".seldialog.list.olistbox yview"
    button .seldialog.moves.move -text "< Add <" -command {
			foreach elem [ .seldialog.list.olistbox curselection ] {
				LDIC_addEntry [ .seldialog.list.olistbox get $elem ]
			}}
    button .seldialog.moves.selall -text " Select all  " -command LDIC_selectAll

    foreach elem $offered_list {
      .seldialog.list.olistbox insert end $elem
    }
  }
  label .seldialog.entry.label -text "$prompt"
  entry .seldialog.entry.entry
  bind .seldialog.entry.entry <F1> "LDIC_helpOnListDialog $prompt $extended"
  bind .seldialog.entry.entry <Return> { LDIC_addEntry [ .seldialog.entry.entry get ] }

  button .seldialog.cmds.cancel -text Cancel -command "set tkPriv(sel_marker) -1"
  button .seldialog.cmds.done -text Done -command "set tkPriv(sel_marker) 0"

  if { ! $extended } {
    pack .seldialog.list.listbox .seldialog.list.scrollbar -side left -fill both
  } else {
    pack .seldialog.list.listbox .seldialog.list.scrollbar .seldialog.list.olistbox .seldialog.list.oscrollbar -side left -fill both
    pack .seldialog.labels.sel .seldialog.labels.off -side left -expand 1
  }
  pack .seldialog.list.listbox -expand 1

  foreach listelem $sorted_list {
    .seldialog.list.listbox insert end $listelem
  }
  if { ! $extended } {
    pack .seldialog.moves.add .seldialog.moves.remove -side left -expand 1
  } else {
    pack .seldialog.moves.add .seldialog.moves.remove .seldialog.moves.move .seldialog.moves.selall -side left -expand 1
  }
  pack .seldialog.entry.label .seldialog.entry.entry -side left
  pack .seldialog.entry.entry -fill x -expand 1
  pack .seldialog.cmds.done .seldialog.cmds.cancel -side left -expand 1
  pack .seldialog.labels .seldialog.list .seldialog.moves .seldialog.entry .seldialog.cmds -side top -fill both -pady 2

  wm withdraw $w
  GGUI_placeToplevel $w 1
  update idletasks
  wm deiconify $w 
  set old_focus [focus]
  set old_grab [ grab current $w]
  if {$old_grab != ""} {
        set grab_status [grab status $old_grab]
  }
  grab $w
  tkwait variable tkPriv(sel_marker)

  if {$old_grab != ""} {
    if {$grabStatus == "global"} {
      grab -global $old_grab
    } else {
      grab $old_grab
    }
  }

  set sel $tkPriv(sel_marker)
  set selection ""
  if { $sel >= 0 } {
    set selection [ .seldialog.list.listbox get 0 end ]
  }

  destroy $w

  if { $sel < 0 } {
    return "--"
  }

  return $selection
}

#
# Pop up dialog with 1st arg as window title for entering a value.
# The entry is preceded by the 2nd arg as prompt. 3rd arg determines
# entry width in characters. 4th arg is the variable the entry value
# is taken from and stored in afterwards. Return value is 0, if ok.
#
proc GGUI_enterDialog { title prompt width val } {
  global tkPriv GVar_edicEntry
  upvar $val value

  set tkPriv(sel_marker) -2

  set w .enterdialog

  catch { destroy $w }
  toplevel $w -class Dialog
  wm title $w $title
  wm protocol $w WM_DELETE_WINDOW "set tkPriv(sel_marker) -3"
  wm iconname $w $title
#  wm transient $w [ winfo toplevel [ winfo parent $w ] ]
  frame .enterdialog.entry
  frame .enterdialog.cmds
  label .enterdialog.entry.label -text $prompt
  entry .enterdialog.entry.entry -width $width
  if { [ info exists value ] } {
    .enterdialog.entry.entry insert end $value
  }
  bind .enterdialog.entry.entry <Return> { set GVar_edicEntry [ .enterdialog.entry.entry get ] ; set tkPriv(sel_marker) 0 }
#  bind .enterdialog.entry.entry <F1> help_on_edic
  button .enterdialog.cmds.do -text Done -command { set GVar_edicEntry [ .enterdialog.entry.entry get ] ; set tkPriv(sel_marker) 0 }
  button .enterdialog.cmds.cancel -text Cancel -command "set tkPriv(sel_marker) -1"

  pack .enterdialog.entry.label .enterdialog.entry.entry -side top -anchor w
  pack .enterdialog.cmds.do .enterdialog.cmds.cancel -side left -expand 1
  pack .enterdialog.entry .enterdialog.cmds -side top -fill both -pady 2

  wm withdraw $w
  GGUI_placeToplevel $w
  update idletasks
  wm deiconify $w 
  set old_focus [focus]
  set old_grab [ grab current $w]
  if {$old_grab != ""} {
        set grab_status [grab status $old_grab]
  }
  grab $w
  tkwait variable tkPriv(sel_marker)

  if {$old_grab != ""} {
    if {$grabStatus == "global"} {
      grab -global $old_grab
    } else {
      grab $old_grab
    }
  }

  destroy $w

  set sel $tkPriv(sel_marker)
  if { $sel < 0 } {
    return 1
  }

  set value $GVar_edicEntry

  return 0
}

#
# generic dialog like basic TCL/TK-tk_dialog
#
# toplevel widget name, title, displayed text, displayed bitmap, default idx,
#   button texts
#
proc GGUI_genericDialog {w title text bitmap default args} {
    global tkPriv genericDialogWidth

    if { [ info exists genericDialogWidth ] } {
	set dialogWidth $genericDialogWidth
    } else {
	set dialogWidth [ expr sqrt([ string length $text ]) * 15 ]
	if { $dialogWidth < 150 } {
	  set dialogWidth 150
	}
    }

    catch {destroy $w}
    toplevel $w -class Dialog
    wm title $w $title
    wm iconname $w Dialog
    wm protocol $w WM_DELETE_WINDOW { }

    frame $w.bot -relief raised -bd 1
    pack $w.bot -side bottom -fill both
    frame $w.top -relief raised -bd 1
    pack $w.top -side top -fill both -expand 1

    option add *Dialog.msg.wrapLength $dialogWidth widgetDefault
    label $w.msg -justify left -text $text
    catch {$w.msg configure -font \
		-Adobe-Times-Medium-R-Normal--*-180-*-*-*-*-*-*
    }
    pack $w.msg -in $w.top -side right -expand 1 -fill both -padx 3m -pady 3m
    if {$bitmap != ""} {
	label $w.bitmap -bitmap $bitmap
	pack $w.bitmap -in $w.top -side left -padx 3m -pady 3m
    }

    set i 0
    foreach but $args {
	button $w.button$i -text $but -command "set tkPriv(button) $i"
	if {$i == $default} {
	    frame $w.default -relief sunken -bd 1
	    raise $w.button$i $w.default
	    pack $w.default -in $w.bot -side left -expand 1 -padx 3m -pady 2m
	    pack $w.button$i -in $w.default -padx 2m -pady 2m
	} else {
	    pack $w.button$i -in $w.bot -side left -expand 1 \
		    -padx 3m -pady 2m
	}
	incr i
    }

    if {$default >= 0} {
	bind $w <Return> "
	    $w.button$default configure -state active -relief sunken
	    update idletasks
	    after 100
	    set tkPriv(button) $default
	"
    }

    wm withdraw $w
    GGUI_placeToplevel $w
    update idletasks
    wm deiconify $w

    set oldFocus [focus]
    set oldGrab [grab current $w]
    if {$oldGrab != ""} {
	set grabStatus [grab status $oldGrab]
    }
    grab $w
    if {$default >= 0} {
	focus $w.button$default
    } else {
	focus $w
    }

    tkwait variable tkPriv(button)
    catch {focus $oldFocus}
    destroy $w
    if {$oldGrab != ""} {
	if {$grabStatus == "global"} {
	    grab -global $oldGrab
	} else {
	    grab $oldGrab
	}
    }
    return $tkPriv(button)
}

#
# internal callback
proc LDIC_set_filename_from_list { w y } {
  global	GGUI_fileSelDialog_filename GGUI_fileSelDialog_actpath
  set sel [ $w get [ $w nearest $y ] ]

  if { $sel == ".." } {
    return
  }

  set GGUI_fileSelDialog_filename [ GFile_cleanPath "$GGUI_fileSelDialog_actpath/$sel" ]
}

#
# internal callback
proc LDIC_set_lists_from_filter { w w2 } {
  global	GGUI_fileSelDialog_filter

  $w delete 0 end
  $w2 delete 0 end
  set entries [ GFile_matchPath $GGUI_fileSelDialog_filter ]
  foreach dir ".. [ lsort [ lindex $entries 1 ] ]" {
    $w insert end $dir
  }
  foreach dir [ lsort [ lindex $entries 0 ] ] {
    $w2 insert end $dir
  }
}

#
# internal callback
proc LDIC_set_path_from_list { w w2 y } {
  global	GGUI_fileSelDialog_filename GGUI_fileSelDialog_actpath
  global	GGUI_fileSelDialog_filter

  set sel [ $w get [ $w nearest $y ] ]
  if { $sel == ".." } {
    set GGUI_fileSelDialog_actpath [ file dirname $GGUI_fileSelDialog_actpath ]
  } else {
    set GGUI_fileSelDialog_actpath "$GGUI_fileSelDialog_actpath/$sel"
  }
  set GGUI_fileSelDialog_actpath [ GFile_cleanPath $GGUI_fileSelDialog_actpath ]

  set GGUI_fileSelDialog_filename $GGUI_fileSelDialog_actpath

  set filter_trailer [ file tail $GGUI_fileSelDialog_filter ]

  set GGUI_fileSelDialog_filter [ GFile_cleanPath "$GGUI_fileSelDialog_actpath/$filter_trailer" ]

  LDIC_set_lists_from_filter $w $w2
}

#
# help stuff
proc LDIC_help_on_filesel_dialog { } {
  GGUI_genericDialog .help "Help on: File Selection Dialog" "A single click to an item of either the directory list on the left or the filename list on the right side selects the item, so it appears in the filename entry underneath the lists. A double click on a directory list item will change to that directory. A double click on a filename will select this filename and close the dialog. Pressing <Return> in the filename field also selects the current entry and closes the dialog. The filter entry can be modified and pressing the <Return> key will put the current filter entry into effect." "" 0 Ok
}


#
# Display a file selection dialog. 1st arg is the dialog title
# second arg is the label for the select-it-button, third arg
# is a variable name to store the current path, useful, if the
# user navigated around and expects to be in the same directory
# when this dialog opens next time
#
proc GGUI_fileSelDialog { title sel_button_label path } {
  global	tkPriv GGUI_fileSelDialog_actpath GGUI_fileSelDialog_filter
  global	GGUI_fileSelDialog_filename
  upvar $path pathvar

  if { ! [ info exists pathvar ] } {
    set fp [ open "|pwd" r ]
    gets $fp pathvar
    close $fp
  }

  set pathvar [ GFile_cleanPath $pathvar ]

  set GGUI_fileSelDialog_actpath $pathvar
  set GGUI_fileSelDialog_filter "[ GFile_cleanPath $GGUI_fileSelDialog_actpath/* ]"
  set GGUI_fileSelDialog_filename $GGUI_fileSelDialog_actpath

  set tkPriv(sel_marker) -2

  set w .fileseldialog

  catch { destroy $w }
  toplevel $w -class Dialog
  wm title $w $title
  wm protocol $w WM_DELETE_WINDOW "set tkPriv(sel_marker) -3"
  wm iconname $w $title
#  wm transient $w [ winfo toplevel [ winfo parent $w ] ]
  frame $w.filter
  frame $w.header
  frame $w.sel
  frame $w.sel.dir
  frame $w.sel.file
  frame $w.filename
  frame $w.cmds

  set scrollb 0
  set height 20
  label $w.filter.label -text "Filter: "
  entry $w.filter.entry -textvariable GGUI_fileSelDialog_filter
  label $w.header.dir -text "Directories:" -anchor w
  label $w.header.file -text "Files:      " -anchor w
  listbox $w.sel.dir.listbox -height $height -selectmode single -yscrollcommand "$w.sel.dir.scrollbar set"
  scrollbar $w.sel.dir.scrollbar -command "$w.sel.dir.listbox yview"
  listbox $w.sel.file.listbox -height $height -selectmode single -yscrollcommand "$w.sel.file.scrollbar set"
  scrollbar $w.sel.file.scrollbar -command "$w.sel.file.listbox yview"
  label $w.filename.label -text "Filename: "
  entry $w.filename.entry -textvariable GGUI_fileSelDialog_filename
  button $w.cmds.button1 -text $sel_button_label -command "set tkPriv(sel_marker) 0"
  button $w.cmds.button2 -text "Cancel" -command "set tkPriv(sel_marker) -1"
  button $w.cmds.help -text "Help" -command LDIC_help_on_filesel_dialog
  bind $w.filter.entry <Return> "LDIC_set_lists_from_filter $w.sel.dir.listbox $w.sel.file.listbox"
  bind $w.filename.entry <Return> "set tkPriv(sel_marker) 0"
  bind $w.sel.dir.listbox <ButtonPress-1> "LDIC_set_filename_from_list $w.sel.dir.listbox %y"
  bind $w.sel.dir.listbox <Double-ButtonPress-1> "LDIC_set_path_from_list $w.sel.dir.listbox $w.sel.file.listbox %y"
  bind $w.sel.file.listbox <ButtonPress-1> "LDIC_set_filename_from_list $w.sel.file.listbox %y"
  bind $w.sel.file.listbox <Double-ButtonPress-1> "set tkPriv(sel_marker) 1"

  pack $w.filter.label $w.filter.entry -side left -fill x
  pack $w.filter.entry -expand 1
  pack $w.header.dir $w.header.file -side left -fill x -expand 1
  pack $w.sel.dir.listbox $w.sel.dir.scrollbar -side left -fill both
  pack $w.sel.dir.listbox -expand 1
  pack $w.sel.file.listbox $w.sel.file.scrollbar -side left -fill both
  pack $w.sel.file.listbox -expand 1
  pack $w.sel.dir $w.sel.file -side left -fill both -expand 1 -padx 2
  pack $w.filename.label $w.filename.entry -side left -fill x
  pack $w.filename.entry -expand 1
  pack $w.cmds.button1 $w.cmds.button2 $w.cmds.help -side left -expand 1
  pack $w.filter $w.header $w.sel $w.filename $w.cmds -fill both -expand 1 -pady 2 -padx 2

  set entries [ GFile_matchPath $GGUI_fileSelDialog_filter ]
  foreach dir ".. [ lsort [ lindex $entries 1 ] ]" {
    $w.sel.dir.listbox insert end $dir
  }
  foreach dir [ lsort [ lindex $entries 0 ] ] {
    $w.sel.file.listbox insert end $dir
  }

  wm withdraw $w
  GGUI_placeToplevel $w 1
  update idletasks
  wm deiconify $w 
  set old_focus [focus]
  set old_grab [ grab current $w]
  if {$old_grab != ""} {
        set grab_status [grab status $old_grab]
  }
  grab $w
  tkwait variable tkPriv(sel_marker)

  if {$old_grab != ""} {
    if {$grabStatus == "global"} {
      grab -global $old_grab
    } else {
      grab $old_grab
    }
  }

  destroy $w

  set pathvar $GGUI_fileSelDialog_actpath

  if { $tkPriv(sel_marker) < 0 } {
    return ""
  }

  return $GGUI_fileSelDialog_filename
}

#
# internal callback
proc GDIC_saveOutputIfPossible { textwidget } {
  set file [ GGUI_fileSelDialog "Save Command Output" "Save" GDIC_runCmdSaveOutPath ]
  if { $file != "" } {
    if { [ file exists $file ] } {
      if { [ GGUI_genericDialog .confirm "Confirm" "File `$file' exists. Overwrite ?" "" 1 Yes No ] != 0 } {
	return
      }
    }

    set c [ $textwidget get 0.0 end ]
    set r [ GFile_writeFile $file c 1 string ]
    if  { $r } {
      GGUI_errorDialog "Cannot write to file $file."
    }
  }
}

#
# Run the command passed in the 2nd arg and show the output
# in a window with the title passed in the 1st arg. The
#  running command is terminated by SIGTERM, when closing
# the window. If a different kill command should be used, it
# must be supplied as 3rd argument e.g. "/bin/kill -QUIT"
#
proc GGUI_runCommand { args } {
  global	tkPriv LDIC_cmdInterrupted LDIC_commandFp LDIC_fileventProc

  set title [ lindex $args 0 ]
  set cmd [ lindex $args 1 ]
  set killcmd "/bin/kill"
  if { [ llength $args ] > 2 } {
    set killcmd [ lindex $args 2 ]
  }

  set err [ catch { set LDIC_commandFp [ open "|$cmd" ] } ]
  if { $err } {
    GGUI_errorDialog "Cannot run command $cmd."
    return -1
  }

  set tkPriv(run_dialog_open) 1
  set tkPriv(command_running) 1

  set pids [ pid $LDIC_commandFp ]

  set w .procoutwin

  # all the following sets fproc to:
  #
  #  set n [ gets $LDIC_commandFp line ]
  #  if { $n < 0 } {
  #   set tkPriv(command_running) 0
  #   catch { close $LDIC_commandFp }
  #  } else {
  #   append line "\n"
  #   $w.disp.text configure -state normal
  #   $w.disp.text insert end $line
  #   $w.disp.text see end
  #   $w.disp.text configure -state disabled
  #   update idletasks
  #  }
  #
  # where $w will be resolved here

  set fproc {
		set n [ gets $LDIC_commandFp line ]
		if { $n < 0 } {
		  set tkPriv(command_running) 0
		  catch { close $LDIC_commandFp }
		} else }
   append fproc "{
		  append line \"\\n\"
		  $w.disp.text configure -state normal
		  "
  append fproc "$w.disp.text insert end "
  append fproc {$line
		  }
  append fproc "$w.disp.text see end
		  $w.disp.text configure -state disabled
		  update idletasks
		}"

  set LDIC_fileventProc $fproc

  catch { destroy $w }
  toplevel $w -class Dialog
  wm title $w $title

  frame $w.disp
  frame $w.cmds

  text $w.disp.text -state disabled -yscrollcommand "$w.disp.scrollbar set"
  scrollbar $w.disp.scrollbar -command "$w.disp.text yview"

  button $w.cmds.stop -text Stop -command "exec /bin/kill -STOP $pids ; $w.cmds.close configure -state normal ; $w.cmds.cont configure -state normal ; $w.cmds.stop configure -state disabled ; set LDIC_cmdInterrupted 1 ; fileevent $LDIC_commandFp readable {}"
  set contcmd "exec /bin/kill -CONT $pids ; $w.cmds.close configure -state disabled ; $w.cmds.cont configure -state disabled ; $w.cmds.stop configure -state normal ; set LDIC_cmdInterrupted 0 ; fileevent $LDIC_commandFp readable "
  append contcmd {$LDIC_fileventProc}
  button $w.cmds.cont -text Continue -command $contcmd -state disabled
  button $w.cmds.save -text "Save Output" -command "GDIC_saveOutputIfPossible $w.disp.text"
  set confirmcmd { ( ! $LDIC_cmdInterrupted || [ GGUI_genericDialog .confirm "Confirm" "Do you really want to quit ?" "" 1 Yes No ] )}
  button $w.cmds.close -text Close -command "if { $confirmcmd == 0 } { exec /bin/kill -CONT $pids ; exec /bin/kill $pids ; set tkPriv(run_dialog_open) 0 ; set tkPriv(command_running) 0 ; set LDIC_cmdInterrupted 1 }" -state disabled

  pack $w.disp.text $w.disp.scrollbar -side left -fill both
  pack $w.disp.text -expand 1
  pack $w.cmds.stop $w.cmds.cont $w.cmds.save $w.cmds.close -side left -expand 1
  pack $w.disp $w.cmds -side top -fill both -pady 2
  pack $w.disp -expand 1

  set LDIC_cmdInterrupted 0

  while { ! $LDIC_cmdInterrupted } {
    fileevent $LDIC_commandFp readable $LDIC_fileventProc

    tkwait variable tkPriv(command_running)

    $w.cmds.close configure -state normal -command {set tkPriv(run_dialog_open) 0}
    $w.cmds.stop configure -state disabled

    if { ! $LDIC_cmdInterrupted } {
	tkwait variable tkPriv(run_dialog_open)
	break
    }

    break
  }

  catch { close $LDIC_commandFp }

  destroy $w
}

proc CFW_displayConfWidget { parent paramlist } {
  set widgets ""

  set num_params [ llength $paramlist ]

  for { set i 0 } { $i < $num_params } { incr i } {
    set fwidgets ""

    set paramconf [ lindex $paramlist $i ]

    set label [ lindex $paramconf 3 ]
    set varname [ lindex $paramconf 0 ]
    set old_varname [ lindex $paramconf 1 ]
    set annot_proc [ lindex $paramconf 5 ]
    set choice_proc [ lindex $paramconf 6 ]
    set default_proc [ lindex $paramconf 4 ]
    set type [ lindex $paramconf 2 ]

    global $varname $old_varname
    set $old_varname [ set $varname ]

#    if { $default_proc != "" } {
#      set varname [ $default_proc ]
#    }

    frame "$parent.param$i"
    label "$parent.param$i.label" -text $label -width 15
    lappend fwidgets "$parent.param$i.label"

    switch $type {
     s {
      entry "$parent.param$i.entry" -textvariable $varname -bg #aaaaaa -width 35
     }
     b {
      checkbutton "$parent.param$i.entry" -variable $varname -onvalue 1 -offvalue 0
     }
     i {
      frame "$parent.param$i.entry"
     }
    }
    lappend fwidgets "$parent.param$i.entry"


    bind "$parent.param$i.entry" <KeyRelease> "CFW_show_if_changed $parent.param$i.entry $varname $old_varname"
    bind "$parent.param$i.entry" <ButtonRelease> "CFW_show_if_changed $parent.param$i.entry $varname $old_varname"

    lappend paramconf "$parent.param$i"
    set paramlist [ lreplace $paramlist $i $i $paramconf ]

    set annot ""
    if { $annot_proc != "" } {
      set annot [ $annot_proc ]
    }
    label "$parent.param$i.annot" -text $annot

    if { $choice_proc != "" } {
      set choice_cmd "set v "
      append choice_cmd {[ } "$choice_proc $varname $i" {]
		if } "{ " {$v} " != " {""} " } { set $varname " {$v} " }"
#      if { $old_varname != "" } {
	append choice_cmd {
		} "CFW_show_if_changed $parent.param$i.entry $varname $old_varname"
#      }

      button "$parent.param$i.choice" -text "Choose ..." -command $choice_cmd

      pack "$parent.param$i.label" "$parent.param$i.entry" "$parent.param$i.annot" "$parent.param$i.choice" -side left -fill y -anchor w

      lappend fwidgets "$parent.param$i.choice" "$parent.param$i.label"
    } else {
      pack "$parent.param$i.label" "$parent.param$i.entry" "$parent.param$i.annot" -side left -fill y -anchor w

      lappend fwidgets "$parent.param$i.label"
    }

    pack "$parent.param$i" -in $parent -side top -anchor w

    lappend widgets $fwidgets
  }

  return $widgets
}

proc CFW_show_if_changed { widget varname1 varname2 } {
  global	$varname1 $varname2

  set v1 [ set $varname1 ]
  set v2 [ set $varname2 ]

  if { $v1 != $v2 } {
    $widget configure -bg #bb9999
  } else {
    $widget configure -bg #aaaaaa
  }
}

#
# Returns the number of days in the month mon (starting with 1)
# in the year (please in 4 digits)
#
proc GTime_getMonthdays { mon year } {
  set monthdays "31 28 31 30 31 30 31 31 30 31 30 31"

  if { [ expr int($year / 4) ] == [ expr double($year) / 4 ] } {
    set monthdays "31 29 31 30 31 30 31 31 30 31 30 31"
  }
  if { [ expr int($year / 100) ] == [ expr double($year) / 100 ] } {
    set monthdays "31 28 31 30 31 30 31 31 30 31 30 31"
  }
  if { [ expr int($year / 400) ] == [ expr double($year) / 400 ] } {
    set monthdays "31 29 31 30 31 30 31 31 30 31 30 31"
  }

  incr mon -1
  return [ lindex $monthdays $mon ]
}

proc GMath_fitRange { varp min max } {
  upvar	$varp var

  if { $var == "" } return

  set errfl [ catch { format %d $var } ]
  if { $errfl } {
    set var $min
  } else {
    regsub {^0*([1-9].*)} $var {\1} nvar
    if { $nvar != $var } {
      set var $nvar
    }
    if { $var < $min } {
      set var $min
    } else {
      if { $var > $max } {
	set var $max
      }
    }
  }
}

#
# returns the short name and index number of the day of the
# week specified by day, month number (starting with 1) and
# year (please, in 4 digits ;-) )
#
# day index is: 0=Sunday 1=Monday ...
#
proc GTime_getWeekday { day mon year } {
  set monthdays "31 28 31 30 31 30 31 31 30 31 30 31"

  if { [ expr int($year / 4) ] == [ expr double($year) / 4 ] } {
    set monthdays "31 29 31 30 31 30 31 31 30 31 30 31"
  }
  if { [ expr int($year / 100) ] == [ expr double($year) / 100 ] } {
    set monthdays "31 28 31 30 31 30 31 31 30 31 30 31"
  }
  if { [ expr int($year / 400) ] == [ expr double($year) / 400 ] } {
    set monthdays "31 29 31 30 31 30 31 31 30 31 30 31"
  }

  set y [ expr $year - 1 ]

  set d [ expr $y * 365 + int($y / 4) - int($y / 100) + int($y / 400) ]

  incr mon -1

  for { set i 0 } { $i < $mon } { incr i } {
    incr d [ lindex $monthdays $i ]
  }

  set d [ expr ($d + $day) % 7 ]

  return [ list [ GDate_getDayname $d ] $d ]
}

proc GDate_getDayname { idx } {
  return [ lindex "Sun Mon Tue Wed Thu Fri Sat" [ expr $idx % 7 ] ]
}

proc GDate_getMonthname { idx } {
  return [ lindex "Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec" [ expr ($idx - 1) % 12 ] ]
}

proc GDIC_check_timeanddate { dayp monthp yearp wdaywidget } {
  upvar $dayp mday
  upvar $monthp month
  upvar $yearp year

  if { $month == "" || $year == "" || $mday == "" } {
    return
  }

  set errfl [ catch { format %d $year } ]
  if { $errfl } {
    set year 1970
  } else {
    regsub {^0*([1-9].*)} $year {\1} nyear
    if { $year != $nyear } {
      set year $nyear
    }
  }

  GMath_fitRange month 1 12

  # check day
  set errfl [ catch { set maxday [ GTime_getMonthdays $month $year ] } ]
  if { ! $errfl } {
    set errfl [ catch { format %d $mday } ]
    if { $errfl } {
      set mday 1
    } else {
      regsub {^0*([0-9].*)} $mday {\1} nmday
      if { $mday != $nmday } {
	set mday $nmday
      }
      if { $mday < 1 } {
	set mday 1
      } else {
	if { $mday > $maxday } {
	  set mday $maxday
	}
      }
    }

    set weekday [ GTime_getWeekday $mday $month $year ]
    $wdaywidget configure -text " [ lindex $weekday 0 ]"
  }
}

proc GGUI_chooseTimeDateWidget { parent mp hp dp Mp yp } {
  global	$mp $hp $dp $Mp $yp

  set errfl [ catch { set weekday [ lindex [ GTime_getWeekday [ set $dp ] [ set $Mp ] [ set $yp ] ] 0 ] } ]
  if { $errfl } {
    set weekday "Day"
  }

  frame $parent.frame
  frame $parent.frame.time
  label $parent.frame.time.label -text "Time: "
  entry $parent.frame.time.hour -width 2 -textvariable $hp
  label $parent.frame.time.labelc -text ":"
  entry $parent.frame.time.min -width 2 -textvariable $mp
  bind $parent.frame.time.hour <KeyRelease> "GMath_fitRange $hp 0 23"
  bind $parent.frame.time.min <KeyRelease> "GMath_fitRange $mp 0 59"

  frame $parent.frame.date
  label $parent.frame.date.label -text "Date: "
  entry $parent.frame.date.mday -width 2 -textvariable $dp
  label $parent.frame.date.label2 -text "."
  entry $parent.frame.date.month -width 2 -textvariable $Mp
  label $parent.frame.date.label3 -text "."
  entry $parent.frame.date.year -width 4 -textvariable $yp
  label $parent.frame.date.wday -text " $weekday" -anchor e
  bind $parent.frame.date.mday <KeyRelease> "GDIC_check_timeanddate $dp $Mp $yp $parent.frame.date.wday"
  bind $parent.frame.date.month <KeyRelease> "GDIC_check_timeanddate $dp $Mp $yp $parent.frame.date.wday"
  bind $parent.frame.date.year <KeyRelease> "GDIC_check_timeanddate $dp $Mp $yp $parent.frame.date.wday"

  pack $parent.frame.time.label $parent.frame.time.hour $parent.frame.time.labelc $parent.frame.time.min -side left
  pack $parent.frame.date.label $parent.frame.date.mday $parent.frame.date.label2 $parent.frame.date.month $parent.frame.date.label3 $parent.frame.date.year $parent.frame.date.wday -side left
  pack $parent.frame.time $parent.frame.date -side top -fill x
#  pack $parent.frame

  return $parent.frame
}

#
# GUI stuff
if { ! [ info exists Conf_listHeight ] } {
  set Conf_listHeight 20
}

#
# Linux stuff
set GCon_distribution "unknown"
if { [ info exists tcl_platform ] } {
  if { [ regexp -nocase linux $tcl_platform(os) ] } {
    if { [ file isdirectory /etc/rc.d ] } {
	set GCon_distribution "slackware"
    }
    if { [ file isfile /etc/SuSE-release ] } {
	set GCon_distribution "suse"
    }
    if { [ file isfile /etc/debian_version ] } {
	set GCon_distribution "debian"
    }
    if { $GCon_distribution == "unknown" } {
	GIO_errorMessage "Unknown Linux distribution. Some functionality will not be available"
    }
  }
}

#
# Internationalization stuff
proc INTL_gettext { text { textdomain "" } { textdomaindir "" } } {
  global	INTL_gettext_cache INTL_gettext_program
  global	INTL_gettext_domains INTL_gettext_domaindirs

  if { [ string trim $text ] == "" } {
    return $text
  }

  if { [ info exists INTL_gettext_cache($text) ] && [ info exists INTL_gettext_domains($text) ] && [ info exists INTL_gettext_domaindirs($text) ] } {
    if { $INTL_gettext_domains($text) == $textdomain && $INTL_gettext_domaindirs($text) == $textdomaindir } {
      return $INTL_gettext_cache($text)
    }
  }

  if { [ info exists INTL_gettext_program ] } {
    if { $INTL_gettext_program == "" } {
      return $text
    }

    set gtext [ GStr_replSubstring $text "'" {'"'"'} ]
    set rettext [ exec /bin/sh -c "TEXTDOMAIN='$textdomain'; TEXTDOMAINDIR='$textdomaindir'; export TEXTDOMAIN TEXTDOMAINDIR; $INTL_gettext_program '$gtext'" ]

    set INTL_gettext_cache($text) $rettext
    set INTL_gettext_domains($text) $textdomain
    set INTL_gettext_domaindirs($text) $textdomaindir

    return $rettext
  }

  set INTL_gettext_program [ GFile_findProgram gettext ]

  return [ INTL_gettext $text $textdomain $textdomaindir ]
}
