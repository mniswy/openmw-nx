#!/usr/bin/perl
# Writes a copy of the build's compile_commands.json for editors running on Windows, such as CLion, for
# build.sh.
#
#     compile_commands.pl <source directory> <build directory> <output file> <path>=<Windows path>...
#
# CMake runs inside devkitPro's shell, so the commands it lists name files the way that shell does
# (/c/Users/..., /opt/devkitpro/...), which programs outside it cannot open. The copy spells each given
# path the Windows way, and lists OpenMW's own source files only: the libraries built along with it
# (OpenSceneGraph, MyGUI, Bullet and the rest) would only make the editor take longer to read the project,
# and their headers are found through the commands that remain.
#
# In Perl because devkitPro's shell on Windows comes with it, and not with Python.

use strict;
use warnings;
use JSON::PP;

die "usage: $0 <source directory> <build directory> <output file> <path>=<Windows path>...\n" unless @ARGV >= 3;
my ($source, $build, $output, @mappings) = @ARGV;

my @replacements;
for my $mapping (@mappings) {
    die "$0: not <path>=<Windows path>: $mapping\n" unless $mapping =~ /^(.+?)=(.+)$/;
    push @replacements, [ $1, $2 ];
}
# The build directory is usually inside the source directory, so the longer of the two goes first.
@replacements = sort { length($b->[0]) <=> length($a->[0]) } @replacements;

sub windows_paths {
    my ($text) = @_;
    for my $replacement (@replacements) {
        my ($from, $to) = @$replacement;
        $text =~ s/\Q$from\E(?=[\/"\s]|$)/$to/g;
    }
    return $text;
}

my $json = JSON::PP->new->utf8;
open(my $in, '<:raw', "$build/compile_commands.json") or die "$0: $build/compile_commands.json: $!\n";
my $commands = $json->decode(do { local $/; <$in> });
close($in);

my @kept = grep { index($_->{file}, "$source/") == 0 && index($_->{file}, "$build/") != 0 } @$commands;
for my $command (@kept) {
    for my $value (values %$command) {
        if (ref $value eq 'ARRAY') {
            $_ = windows_paths($_) for @$value;
        } else {
            $value = windows_paths($value);
        }
    }
}

# Written next to the file and then moved over it, so that an editor watching it never reads half of it.
open(my $out, '>:raw', "$output.tmp") or die "$0: $output.tmp: $!\n";
print $out $json->pretty->canonical->encode(\@kept);
close($out) or die "$0: $output.tmp: $!\n";
rename("$output.tmp", $output) or die "$0: $output: $!\n";
