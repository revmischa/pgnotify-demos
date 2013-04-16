#!/usr/bin/env perl

use strict;
use warnings;

use AnyEvent;
use AnyMQ;
use AnyMQ::Pg;
use Data::Dumper;

my $bus = AnyMQ->new_with_traits(
    traits     => ['Pg'],
    dsn        => 'dbname=postgres user=postgres',
    # on_connect => sub { ... },
    # on_error   => sub { ... },
);

# see AnyMQ docs for usage
my $topic = $bus->topic('mychannel');
my $listen_watcher = $bus->new_listener($topic);
$listen_watcher->poll(sub {
    my ($evt) = @_;
    warn "Got notified of my_event: " . Dumper($evt) . "\n";
});
$topic->publish({ foo => 123 });
AE::cv->recv;
