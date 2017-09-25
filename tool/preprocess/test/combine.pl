#!/usr/bin/perl -w

print '
INTERFACE:

#include "bar.h"

class Baz;

class Foo : public Bar
{
  // DATA
  int d_data;
  Baz* d_baz;
};

IMPLEMENTATION:

#include "yes.h"
#include <no.h>

class Rambo
{
};

// Try combinations of {PUBLIC|PROTECTED|PRIVATE|} {static|}
// {inline|INLINE|inline NEEDS[]|INLINE NEEDS[]} {virtual|}

';

$count = 0;

foreach my $class ("Foo", "")
  {
    foreach my $public ("PUBLIC", "PROTECTED", "PRIVATE", "")
      {
	foreach my $static ("static", "")
	  {
	    foreach my $inline ("inline", "INLINE", 
                                "inline NEEDS [Rambo,\"yes.h\"]",
				"INLINE NEEDS[Rambo, \"yes.h\"]", "")
	      {
		foreach my $virtual ("virtual", "")
		  {
		    next if ($public ne '') && ($class eq '');
		    next if ($virtual ne '') && (($static ne '') 
						 || ($class eq ''));
		    print "$public $virtual $static $inline\n";
		    print "void ";
		    print "${class}::" if ($class ne '');
		    print "function" . $count++;
		    print "() {}\n\n";
		  }
	      }
	  }
      }
  }
