#!/usr/bin/perl
# Completes the profile files (*.gcda) that `gcov-tool merge-stream` writes, for build.sh --pgo=use.
#
#     summary.pl <number of runs> <directory>
#
# Every profile file starts with a summary of the run it comes from: how many runs there were, and the
# highest count any branch in the program reached. The compiler takes a block of code for hot if its
# count is at least a ten-thousandth of that, and optimises the rest for size. A program writing its own
# profile files fills the summary in when it exits. One that hands its counts over as a stream, as the
# Switch build has to, does not, and gcov-tool leaves both numbers at zero, which would make everything
# that ran at least twice count as hot. So this does what the program would have done.
#
# In Perl because devkitPro's shell on Windows comes with it, and not with Python.

use strict;
use warnings;
use File::Find;

my $data_magic = 0x67636461;    # "gcda"
my $tag_summary = 0xa1000000;
my $tag_arcs = 0x01a10000;

die "usage: $0 <number of runs> <directory>\n" unless @ARGV == 2;
my ($runs, $directory) = @ARGV;

my @files;
find({ wanted => sub { push @files, $File::Find::name if -f && /\.gcda$/ }, no_chdir => 1 }, $directory);
die "$0: no profile files in $directory\n" unless @files;

sub read_file {
    my ($path) = @_;
    open(my $file, '<:raw', $path) or die "$0: $path: $!\n";
    local $/;
    my $content = <$file>;
    close($file);
    # Magic, version, stamp, checksum, and the summary: tag, length, runs, highest count.
    my ($magic, $tag, $length) = unpack('V x12 V V', $content);
    die "$0: $path is not a profile file as gcov-tool writes it\n"
        unless length($content) >= 32 && $magic == $data_magic && $tag == $tag_summary && $length == 8;
    return $content;
}

my $highest = 0;
for my $path (@files) {
    my $content = read_file($path);
    my $position = 32;
    while ($position + 4 <= length($content)) {
        my $tag = unpack('V', substr($content, $position, 4));
        last if $tag == 0;
        # A negative length stands for that many bytes of zeros, which are not stored.
        my $length = unpack('l<', substr($content, $position + 4, 4));
        $position += 8;
        next if $length <= 0;
        die "$0: $path ends in the middle of a record\n" if $position + $length > length($content);
        if ($tag == $tag_arcs) {
            for my $count (unpack('(q<)*', substr($content, $position, $length))) {
                $highest = $count if $count > $highest;
            }
        }
        $position += $length;
    }
}

# The file has 32 bits for it.
my $stored = $highest > 0xffffffff ? 0xffffffff : $highest;
for my $path (@files) {
    open(my $file, '+<:raw', $path) or die "$0: $path: $!\n";
    seek($file, 24, 0);
    print $file pack('V V', $runs, $stored);
    close($file) or die "$0: $path: $!\n";
}

printf("%d profile files, %d run(s), highest count %d\n", scalar(@files), $runs, $highest);
