use strict;

=head1 LICENSE

   Bweb - A Bacula web interface
   Bacula� - The Network Backup Solution

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.

   The main author of Bweb is Eric Bollengier.   
   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula� is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Z�rich,
   Switzerland, email:ftf@fsfeurope.org.

=cut


################################################################
# Manage with Expect the bconsole tool
package Bconsole;
use Expect;
use POSIX qw/_exit/;
use IPC::Open2;
# my $pref = new Pref(config_file => 'brestore.conf');
# my $bconsole = new Bconsole(pref => $pref);
sub new
{
    my ($class, %arg) = @_;
    my $dir = $arg{pref}->{current_conf};
    if ($dir) {
        if (!$arg{pref}->{main_conf}->{bconsole} ||
            ($arg{pref}->{main_conf}->{bconsole} ne  $arg{pref}->{bconsole}))
        {
            # If bconsole string is different, don't use director option
            $dir = undef;
        }
    }
    if ($arg{director}) {
        $dir = $arg{director};
    }
    my $self = bless {
        pref => $arg{pref},     # Pref object
        dir  => $dir,           # Specify the director to connect
        bconsole => undef,      # Expect object
        log_stdout => $arg{log_stdout} || 0,
        timeout => $arg{timeout} || 20,
        debug   => $arg{debug} || 0,
    };

    return $self;
}

sub run
{
    my ($self, %arg) = @_;

    my $cmd = 'run ';
    my $go  = 'yes';

    if ($arg{restore}) {
	$cmd = 'restore ';
	$go  = 'done yes';
	delete $arg{restore};
    }

    for my $key (keys %arg) {
	if ($arg{$key}) {
	    $arg{$key} =~ tr/""/  /;
	    $cmd .= "$key=\"$arg{$key}\" ";
	}
    }

    unless ($self->connect()) {
	return 0;
    }

    print STDERR "===> $cmd $go\n";
    $self->{bconsole}->clear_accum();
    $self->send("$cmd $go\n");
    $self->expect_it('-re','^[*]');
    my $ret = $self->before();
    if ($ret =~ /jobid=(\d+)/is) {
	return $1;
    } else {
	return 0;
    }
}

# for brestore.pl::BwebConsole
sub prepare
{
    # do nothing
}

sub send
{
    my ($self, $what) = @_;
    $self->{bconsole}->send($what);
}

sub expect_it
{
    my ($self, @what) = @_;
    unless ($self->{bconsole}->expect($self->{timeout}, @what)) {
	return $self->error($self->{bconsole}->error());
    }
    return 1;
}

sub log_stdout
{
    my ($self, $how) = @_;

    if ($self->{bconsole}) {
       $self->{bconsole}->log_stdout($how);
    }

    $self->{log_stdout} = $how;
}

sub error
{
    my ($self, $error) = @_;
    $self->{error} = $error;
    if ($error) {
        print STDERR "E: bconsole (", $self->{pref}->{bconsole}, ") $! $error\n";
        print STDERR "I: Check permissions on binary and configuration file\n";
        print STDERR "I: Try to execute it in a terminal with a su command\n";
    }
    return 0;
}

sub connect
{
    my ($self) = @_;

    if ($self->{error}) {
	return 0 ;
    }

    unless ($self->{bconsole}) {
	my @cmd = split(/\s+/, $self->{pref}->{bconsole}) ;
	unless (@cmd) {
	    return $self->error("bconsole string not found");
	}
        if ($self->{dir}) {
            push @cmd, "-D", $self->{dir}; # should handle spaces
        }
        if ($self->{pref}->{debug}) {
            my $c = join(' ', @cmd, '-t');
            my $out = `$c 2>&1`;
            if ($? != 0) {
                print "bconsole test ($c): $out";
            }
            print STDERR "bconsole test ($c) ret=$?: $out";
        }
	$self->{bconsole} = new Expect;
	$self->{bconsole}->raw_pty(1);
	$self->{bconsole}->debug($self->{debug});
	$self->{bconsole}->log_stdout($self->{debug} || $self->{log_stdout});

	# WARNING : die is trapped by gtk_main_loop()
	# and exit() closes DBI connection 
	my $ret;
	{ 
	    my $sav = $SIG{__DIE__};
	    $SIG{__DIE__} = sub {  _exit 1 ;};
            my $old = $ENV{COLUMNS};
            $ENV{COLUMNS} = 300;
	    $ret = $self->{bconsole}->spawn(@cmd) ;
	    delete $ENV{COLUMNS};
	    $ENV{COLUMNS} = $old if ($old) ;
	    $SIG{__DIE__} = $sav;
	}

	unless ($ret) {
	    return $self->error($self->{bconsole}->error());
	}
	
	# TODO : we must verify that expect return the good value

	$self->expect_it('*');
	$self->send_cmd('gui on');
    }
    return 1 ;
}

sub cancel
{
    my ($self, $jobid) = @_;
    return $self->send_cmd("cancel jobid=$jobid");
}

# get text between to expect
sub before
{
    my ($self) = @_;
    return $self->{bconsole}->before();
}

# use open2 to send a command, skip expect, faster
sub send_one_cmd
{
    my ($self, $line) = @_;
    my @ret;
    my $cmd = $self->{pref}->{bconsole} ;
    if ($self->{dir}) {
        $cmd = $cmd . " -D '$self->{dir}'";
    }
    my ($OUT, $IN);
    my $pid = open2($OUT, $IN, $cmd);
    print $IN "$line\n";
    close($IN);
    my $l = <$OUT>;             # Connecting to
    $l = <$OUT>;                # 1000: OK
    $l = <$OUT>;                # Enter a period
    $l = <$OUT>;                # line

    while ($l = <$OUT>) {
        push @ret, $l;
    }
    close($OUT);
    waitpid($pid, 0);
    return wantarray? @ret : \@ret;
}

sub send_cmd
{
    my ($self, $cmd) = @_;
    unless ($self->connect()) {
	return '';
    }
    $self->{bconsole}->clear_accum();
    $self->send("$cmd\n");
    $self->expect_it('-re','^[*]');
    return $self->before();
}

sub send_cmd_yes
{
    my ($self, $cmd) = @_;
    unless ($self->connect()) {
	return '';
    }
    $self->send("$cmd\n");
    $self->expect_it('-re', '[?].+:');

    $self->send_cmd("yes");
    return $self->before();
}

sub label_barcodes
{
    my ($self, %arg) = @_;

    unless ($arg{storage}) {
	return '';
    }

    unless ($self->connect()) {
	return '';
    }

    $arg{drive} = $arg{drive} || '0' ;
    $arg{pool} = $arg{pool} || 'Scratch';

    my $cmd = "label barcodes drive=$arg{drive} pool=\"$arg{pool}\" storage=\"$arg{storage}\"";

    if ($arg{slots}) {
	$cmd .= " slots=$arg{slots}";
    }

    $self->send("$cmd\n");
    $self->expect_it('-re', '[?].+\).*:');
    my $res = $self->before();
    $self->send("yes\n");
#    $self->expect_it("yes");
#    $res .= $self->before();
    $self->expect_it('-re','^[*]');
    $res .= $self->before();
    return $res;
}

#
# return [ { name => 'test1', vol => '00001', ...�},
#          { name => 'test2', vol => '00002', ... }... ] 
#
sub director_get_sched
{
    my ($self, $days) = @_ ;

    $days = $days || 1;

    unless ($self->connect()) {
	return '';
    }
   
    my $status = $self->send_cmd("st director days=$days") ;

    my @ret;
    foreach my $l (split(/\r?\n/, $status)) {
	#Level          Type     Pri  Scheduled        Name       Volume
	#Incremental    Backup    11  03-ao-06 23:05  TEST_DATA  000001
	if ($l =~ /^(I|F|Di)\w+\s+\w+\s+\d+/i) {
	    my ($level, $type, $pri, $d, $h, @name_vol) = split(/\s+/, $l);

	    my $vol = pop @name_vol; # last element
	    my $name = join(" ", @name_vol); # can contains space

	    push @ret, {
		level => $level,
		type  => $type,
		priority => $pri,
		date  => "$d $h",
		name  => $name,
		volume => $vol,
	    };
	}

    }
    return \@ret;
}

sub update_slots
{
    my ($self, $storage, $drive) = @_;
    $drive = $drive || 0;

    return $self->send_cmd("update slots storage=$storage drive=$drive");
}

# Return:
#$VAR1 = {
#          'I' => [
#                   {
#                     'file' => '</tmp/regress/tmp/file-list'
#                   },
#                   {
#                     'file' => '</tmp/regress/tmp/other-file-list'
#                   }
#                 ],
#          'E' => [
#                   {
#                     'file' => '</tmp/regress/tmp/efile-list'
#                   },
#                   {
#                     'file' => '</tmp/regress/tmp/other-efile-list'
#                   }
#                 ]
#        };
sub get_fileset
{
    my ($self, $fs) = @_;

    my $out = $self->send_cmd("show fileset=\"$fs\"");
    
    my $ret = {};

    foreach my $l (split(/\r?\n/, $out)) { 
        #              I /usr/local
	if ($l =~ /^\s+([O|I|E|P])\s+(.+)$/) { # include
            # if a plugin, add it to the include list
            if ($1 eq 'P') {
                push @{$ret->{'I'}}, { file => $2 };
            }
	    push @{$ret->{$1}}, { file => $2 };
	}
    }

    return $ret;
}

sub list_backup
{
    my ($self) = @_;
    return sort split(/\r?\n/, $self->send_cmd(".jobs type=B"));
}

sub list_restore
{
    my ($self) = @_;
    return sort split(/\r?\n/, $self->send_cmd(".jobs type=R"));
}

sub list_job
{
    my ($self) = @_;
    return sort split(/\r?\n/, $self->send_cmd(".jobs"));
}

sub set_director
{
    my ($self, $dir) = @_;
    $self->{dir} = $dir;
}

sub list_directors
{
    my ($self) = @_;
    my $lst = `$self->{pref}->{bconsole} -l`;
    if ($? != 0) {
        return $self->error("Director list unsupported by bconsole");
    }
    return sort split(/\r?\n/, $lst);
}

sub list_fileset
{
    my ($self) = @_;
    return sort split(/\r?\n/, $self->send_cmd(".filesets"));
}

sub list_storage
{
    my ($self) = @_;
    return sort split(/\r?\n/, $self->send_cmd(".storage"));
}

sub list_client
{
    my ($self) = @_;
    return sort split(/\r?\n/, $self->send_cmd(".clients"));
}

sub list_pool
{
    my ($self) = @_;
    return sort split(/\r?\n/, $self->send_cmd(".pools"));
}

use Time::ParseDate qw/parsedate/;
use POSIX qw/strftime/;
use Data::Dumper;

sub _get_volume
{
    my ($self, @volume) = @_;
    return '' unless (@volume);

    my $sel='';
    foreach my $vol (@volume) {
	if ($vol =~ /^([\w\d\.-]+)$/) {
	    $sel .= " volume=$1";

	} else {
	    $self->error("Sorry media is bad");
	    return '';
	}
    }

    return $sel;
}

sub purge_volume
{
    my ($self, $volume) = @_;

    my $sel = $self->_get_volume($volume);
    my $ret;
    if ($sel) {
	$ret = $self->send_cmd("purge $sel");
    } else {
	$ret = $self->{error};
    }
    return $ret;
}

sub prune_volume
{
    my ($self, $volume) = @_;

    my $sel = $self->_get_volume($volume);
    my $ret;
    if ($sel) {
	$ret = $self->send_cmd("prune $sel yes");
    } else {
	$ret = $self->{error};
    }
    return $ret;
}

sub purge_job
{
    my ($self, @jobid) = @_;

    return 0 unless (@jobid);

    my $sel='';
    foreach my $job (@jobid) {
	if ($job =~ /^(\d+)$/) {
	    $sel .= " jobid=$1";

	} else {
	    return $self->error("Sorry jobid is bad");
	}
    }

    $self->send_cmd("purge $sel");
}

sub close
{
    my ($self) = @_;
    $self->send("quit\n");
    $self->{bconsole}->soft_close();
    $self->{bconsole} = undef;
}

1;

__END__

# to use this, run backup-bacula-test and run
# grep -v __END__ Bconsole.pm | perl

package main;

use File::Copy 'copy';
use Data::Dumper qw/Dumper/;
use Test::More tests => 22;

my $bin = "/tmp/regress/bin/bconsole";
my $conf = "/tmp/regress/bin/bconsole.conf";
print "Test without conio\n";

ok(copy($conf, "$conf.org"), "Backup original conf");

system("sed 's/Name = .*/Name = \"zog6 dir\"/' $conf > /tmp/1");
system("sed 's/Name = .*/Name = zog5-dir/' $conf >> /tmp/1");
system("grep zog5-dir $conf > /dev/null || cat /tmp/1 >> $conf");

my $c = new Bconsole(pref => {
    bconsole => "$bin -n -c $conf",
},
    debug => 0);

ok($c, "Create Bconsole object");

my @lst = $c->list_directors();
is(scalar(@lst), 3, "Should find 3 directors");
print "directors : ", join(',', @lst), "\n";
$c->set_director(pop(@lst));
is($c->{dir}, "zog6 dir", "Check current director");

@lst = $c->list_fileset();
is(scalar(@lst), 2, "Should find 2 fileset");
print "fileset : ", join(',', @lst), "\n";

@lst = $c->list_job();
is(scalar(@lst), 3, "Should find 3 jobs");
print "job : ",     join(',', @lst), "\n";

@lst = $c->list_restore();
is(scalar(@lst), 1, "Should find 1 jobs");
print "restore : ",     join(',', @lst), "\n";

@lst = $c->list_backup();
is(scalar(@lst), 2, "Should find 2 jobs");
print "backup : ",     join(',', @lst), "\n";

@lst = $c->list_storage();
is(scalar(@lst), 1, "Should find 1 storage");
print "storage : ", join(',', $c->list_storage()), "\n";

my $r = $c->get_fileset($c->list_fileset());
ok(ref $r, "Check get_fileset return a ref");
ok(ref $r->{I}, "Check Include is array");
print Dumper($r);
print "FS Include:\n", join (",", map { $_->{file} } @{$r->{I}}), "\n";
print "FS Exclude:\n", join (",", map { $_->{file} } @{$r->{E}}), "\n";
$c->close();


ok(copy("$conf.org", $conf), "Restore conf");
my $c = new Bconsole(pref => {
    bconsole => "$bin -n -c $conf",
},
    debug => 0);


ok($c, "Create Bconsole object");
@lst = $c->list_directors();

is(scalar(@lst), 1, "Should find 1 directors");
print "directors : ", join(',', @lst), "\n";

@lst  = $c->list_client();
is(scalar(@lst), 1, "Should find 1 client");

@lst  = $c->list_pool();
is(scalar(@lst), 2, "Should find 2 pool");

@lst = $c->list_fileset();
is(scalar(@lst), 2, "Should find 2 fileset");
print "fileset : ", join(',', @lst), "\n";

@lst = $c->list_job();
is(scalar(@lst), 3, "Should find 3 jobs");
print "job : ",     join(',', @lst), "\n";

@lst = $c->list_restore();
is(scalar(@lst), 1, "Should find 1 jobs");
print "restore : ",     join(',', @lst), "\n";

@lst = $c->list_storage();
is(scalar(@lst), 1, "Should find 1 storage");
print "storage : ", join(',', @lst), "\n";

my $r = $c->get_fileset($c->list_fileset());
ok(ref $r, "Check get_fileset return a ref");
ok(ref $r->{I}, "Check Include is array");
print Dumper($r);
print "FS Include:\n", join (",", map { $_->{file} } @{$r->{I}}), "\n";
print "FS Exclude:\n", join (",", map { $_->{file} } @{$r->{E}}), "\n";
$c->close();

