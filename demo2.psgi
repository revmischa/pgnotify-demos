#!/usr/bin/env perl

use strict;
use warnings;

use AnyMQ;
use AnyMQ::Pg;
use Web::Hippie::App::JSFiles;
use Web::Hippie::PubSub;
use Plack::Builder;
use Plack::Request;
use Carp qw/croak cluck/;

# plackup -s Feersum --port 4000 -Ilib -E development -r demo2.psgi
# curl -v http://localhost:4000/_hippie/mxhr/mychannel

# my $dbname = 'person_tracker';
my $dbname = '';

my $mq_bus = AnyMQ->new_with_traits(
    traits   => [ 'Pg' ],
    # dsn      => "dbname=$dbname",
    on_error => \&on_error,
    debug    => 1,
);

sub on_error {
    my ($self, $err, $fatal) = @_;
    warn "Got error: $err";
    fatal($err) if $fatal;
}

my $app = sub {
    my $env = shift;
    my $req = Plack::Request->new($env);
    my $res = $req->new_response(200);

    $res->content_type('text/html; charset=utf-8');

    if ($req->path eq '/') {
        # index
        $res->redirect('/static/person_tracker.html');
    } else {
        # unknown path
        $res->content("Unknown path " . $req->path);
        $res->code(404);
    }

    $res->finalize;
};

builder {
    # static files
    mount '/static' =>
	    Plack::App::Cascade->new(
            apps => [
                Web::Hippie::App::JSFiles->new->to_app,
                Plack::App::File->new( root => 'static' )->to_app,
            ],
        );

    # anymq hippie server
    mount '/_hippie' => builder {
        enable "+Web::Hippie::PubSub",
            keep_alive => 5,
            allow_clientless_publish => 1,
            bus => $mq_bus;
        sub {
            my ($env) = @_;

            my $req = Plack::Request->new($env);
            my $path = $req->path;
            my $channel = $env->{'hippie.args'};

            if ($path eq '/new_listener') {
                warn "Got new listener on channel $channel\n";
            } elsif ($path eq '/message') {
                my $msg = $env->{'hippie.message'};
                warn "Posting message to channel $channel\n";
            } elsif ($path eq '/error') {
                warn "Got hippie error\n";
            } else {
                warn "Unknown hippie event: $path\n";
            }

            return;
        };
    };

    mount '/' => $app;
};


# print stack trace
sub fatal {
    my ($err) = @_;
    cluck "Fatal error when handling request: $err\n";
}
