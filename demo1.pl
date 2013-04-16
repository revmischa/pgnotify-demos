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

# create topic reference
my $topic = $bus->topic('foo');

# create listener for topic
my $listen_watcher = $bus->new_listener($topic);

# set up event callback
$listen_watcher->poll(sub {
    my ($evt) = @_;
    warn "Got notified of foo: " . Dumper($evt) . "\n";
});

# publish an event
$topic->publish({ blargle => 123 });

# block
AE::cv->recv;
