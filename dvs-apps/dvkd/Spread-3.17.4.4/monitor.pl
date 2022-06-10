#!/usr/bin/perl

use ExtUtils::testlib;
use Spread qw(:SP :MESS :ERROR);
use Switch;

$version = Spread::version();
$local_nodeid = 0;
$| = 1;

$nr_nodes = 0;	
$bm_nodes = 0;	

$JOIN=4352;
$LEAVE=5120;
$REGULAR=4;
$GROUP="DDGROUP";
$HELLO_AGENT=100;
$ACKNOWLEDGE=1000;
@src_field = ();
@type_field = ();

$timeout = 10;
$spreaddaemon = "4803";
#=============================================================================
#                        MAIN
#=============================================================================

print "Spread client library version $version\n";
$args{'spread_name'} = $spreaddaemon;
$args{'private_name'} = "DDM.00";
$args{'priority'} = 0;
$args{'group_membership'} = 1;

# CONNECT
print "Trying to connect to spread...\n";
($mbox, $privategroup) = Spread::connect(
	\%args
	);
$tests++;
$fails++ unless defined($mbox);
print "Mailbox is: ".(defined($mbox)?$mbox:"undef")." and ";
print "private_group is ".(defined($privategroup)?$privategroup:"undef")."\n";
print "$sperrno\n" unless defined($mbox);


#JOIN
@groups = ('DDGROUP');
print "Attempting to join: ".join(',', @groups).".\n";
@joined = grep (Spread::join($mbox, $_), @groups);
print "Successfully joined: ".scalar(@joined)." of ".scalar(@groups).
  " group(s) [".join(',', @joined)."].\n";
$tests+=scalar(@groups);
$fails+=(scalar(@groups)-scalar(@joined));

#MULTICAST
$nodeid		= $local_nodeid;
$msg_type	= $HELLO_AGENT;
$nr_nodes	= 0;
$nr_init	= 0;
$message = "$nodeid,$msg_type,$nr_nodes,$nr_init,0,0,0,0";
print "Attempting to multicast ".length($message)." bytes to [@joined[0]].\n";
print "message: $message .\n";
$tests++;
if(($ret = Spread::multicast($mbox, 
			     FIFO_MESS, @joined[0], 100, $message))>0) {
  print "Successfully multicasted $ret bytes to [@joined[0]]\n";
} else {
  print "Failed multicast to [@joined[0]]: $sperrno\n";
  $fails++;
}

#RECEIVE 
print "Entering receive loop. ".
  (defined($timeout)?"(Timeout = ".$timeout."s)\n":"\n");
$tests++;
$ato=0;
$received=0;

while(($st, $s, $g, $mt, $e, $mess) = (defined($timeout))?Spread::receive($mbox, $timeout):Spread::receive($mbox)) {
 $received++;
 if(!defined($st)) {
   print "Receive error: $Spread::sperrorno\n";
   last;
 } 
	print "\nst=[$st] s=[$s] g=[$g] mt=[$mt] mess=[$mess]\n";
# st=[4352] s=[DDGROUP] g=[ARRAY(0x1f244f4)] mt=[0] mess=[e;¢b#DDM.00#node0]	
#  st=[4] s=[#DDM.00#node0] g=[ARRAY(0x204ea24)] mt=[100] mess=[0,100,0,0,0,0,0,0]
# st=[4] s=[#DDA.01#node1] g=[ARRAY(0x204eb14)] mt=[1100] mess=[1,1100,0,0,0,0,0,0]
# st=[5120] s=[DDGROUP] g=[ARRAY(0x1a31d18)] mt=[0] mess=[eÇI£b)#DDA.01#node1]
   if( $st == $JOIN){
		print "JOIN\n";
		print "\tSOURCE: $s\n";
		$a_pos = index($mess, "#DDA.");
		$m_pos = index($mess, "#DDM.");
		print "\ta_pos=$a_pos m_pos=$m_pos\n";
		if( $m_pos >= 0 ) {
			print "\tMONITOR JOIN\n";
		} else {
			if( $a_pos >= 0) {
				$a_pos += 5; 
				$id = substr($mess, $a_pos, 2);
				print "\tAGENT JOIN: id $id\n";	
			} else {		
				print "\tUNKNOWN MEMBER: $s\n";	
			}
		}
   } else {
	   if( $st == $LEAVE){
			print "LEAVE\n";
			print "\tSOURCE: $s\n";
			$a_pos = index($mess, "#DDA.");
			print "\ta_pos=$a_pos\n";
			if( $a_pos >= 0) {
				$a_pos += 5; 
				$id = substr($mess, $a_pos, 2);
				print "\tAGENT LEAVE: id $id\n";	
			} else {		
				print "\tUNKNOWN MEMBER: $s\n";	
			}
	   } else {
		   if ( $st == $REGULAR) {
				print "REGULAR\n";

				@src_field = split(/\#/, $s);
				$type_nr 	 = $src_field[1];
				$node_name   = $src_field[2];
				print "\ttype_nr: $type_nr\n";	
				print "\tnode_name: $node_name\n";	
				

				@type_field = split(/\./, $type_nr);
				$src_type	 = $type_field[0];
				$src_nodeid  = $type_field[1];			
				print "\tsrc_type: $src_type\n";	
				print "\tsrc_nodeid: $src_nodeid\n";	

				if( $privategroup == $s) {
					print "\tSELF MESSAGE - SOURCE: $s\n";		   
				}else{
					print "\tMEMBER MESSAGE - SOURCE: $s\n";		   				
				}
				if( $mt == $HELLO_AGENT){
					print "\tHELLO_AGENT RECEIVED: $mt FROM $src_type WITH NODEID $src_nodeid\n";							
				}
				if( $mt == ($HELLO_AGENT + $ACKNOWLEDGE)){
					print "\tHELLO_MONITOR RECEIVED: $mt FROM $src_type WITH NODEID $src_nodeid\n";
				}
				$nr_nodes++;
				print "\tnr_nodes: $nr_nodes\n";	
				$bm_nodes += 1 << $src_nodeid;
				printf("\tbm_nodes: %08X\n",$bm_nodes);	
		   }
	   } 
   }
}
print "Received $received messages.\n";

#LEAVE
print "Attempting to leave: [".join(',', @joined)."].\n";
$tests+=scalar(@joined);
@left = grep (Spread::leave($mbox, $_) ||
	(($lerror{$_} = $sperrno) && 0), @joined);
$fails+=scalar(@joined)-scalar(@left);
print "Successfully left: [".join(',', @left)."].\n" if(scalar(@left));
while(($k, $v) = each %lerror) {
  print "Failed leave for '$k' beacuse of $v\n";
}
print "EXITING.....\n";
exit(0);

#=============================================================================
#                        FUNCTIONS
#=============================================================================

