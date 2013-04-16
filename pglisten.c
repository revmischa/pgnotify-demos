#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

// channel to LISTEN on
// you can set this via argv, too
const char *listenChannel = "foobar";

void mainLoop(PGconn *conn);
void exitClean(PGconn *conn);
void handlePgRead(PGconn *conn);
void initListen(PGconn *conn);

int main(int argc, char **argv) {
    if (argc == 2) {
        listenChannel = argv[1];
    }
    
    const char* connInfoKeys[] = {
        "host",
        NULL
    };
    const char* connInfoValues[] = {
        "localhost",
        NULL
    };
    
    PGconn *conn = PQconnectStartParams(connInfoKeys, connInfoValues, 0);
    ConnStatusType status = PQstatus(conn);
    if (status == CONNECTION_BAD) {
        fprintf(stderr, "Connection to database failed: %s",
                PQerrorMessage(conn));
        exitClean(conn);
    }
    if (status == CONNECTION_STARTED) {
        printf("Connecting...\n");
    }
    
    mainLoop(conn);

    PQfinish(conn);
}

void exitClean(PGconn *conn) {
    PQfinish(conn);
    exit(1);
}

void mainLoop(PGconn *conn) {
    fd_set rfds, wfds;
    int retval;
    int sock;
    int done = 0;
    int connected = 0;
    int sentListen = 0;
    int connPollReady = 0;
    PostgresPollingStatusType connStatus;
        
    while (! done) {
        // get connection underlying fd
        sock = PQsocket(conn);
        if (sock < 0) {
            printf("Postgres socket is gone\n");
            exitClean(conn);
        }
        
        // initialize select() fd sets
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        
        if (! connected) {
            if (connPollReady) {
                // ready to poll for connection status?
                connStatus = PQconnectPoll(conn);
            
                switch (connStatus) {
                    case PGRES_POLLING_FAILED:
                        // connect failed
                        fprintf(stderr, "Pg connection failed: %s",
                                PQerrorMessage(conn));
                        return;
                    case PGRES_POLLING_WRITING:
                        // ready to send data to server
                        FD_SET(sock, &wfds);
                        break;
                    case PGRES_POLLING_READING:
                        // ready to read back from server
                        FD_SET(sock, &rfds);
                        break;
                    case PGRES_POLLING_OK:
                        // we are now connected and done polling 
                        // the connection state
                        printf("Connected\n");
                        connected = 1;
                        initListen(conn);
                        break;
                }
            } else {
                // wait for sock fd to become writable
                FD_SET(sock, &wfds);
            }
        } 
        
        if (connected) {
            // select on connection becoming readable
            FD_SET(sock, &rfds);
        }
        
        // hang out until there is stuff to read or write
        retval = select(sock + 1, &rfds, &wfds, NULL, NULL);
        switch (retval) {
            case -1:
                perror("select() failed");
                done = 1;
                break;
            default:
                connPollReady = 1;
                
                if (! connected)
                    break;

                // this is always going to be true, but you'll want to
                // check it if you are selecting on more than one fd
                if (FD_ISSET(sock, &rfds)) {
                    // ready to read something interesting
                    handlePgRead(conn);
                }
                break;
        }
    }     
}

void initListen(PGconn *conn) {
    // quote channel identifier
    char *quotedChannel = PQescapeIdentifier(conn, listenChannel, strlen(listenChannel));
    char *query;
    asprintf(&query, "LISTEN %s", quotedChannel);
    
    // send LISTEN query
    int qs = PQsendQuery(conn, query);
    
    // release resources
    PQfreemem(quotedChannel);
    free(query);
    if (! qs) {
        fprintf(stderr, "Failed to send query %s\n", PQerrorMessage(conn));
        return;
    }
}

void handlePgRead(PGconn *conn) {
    PGnotify   *notify;
    PGresult *res;
    PQprintOpt opt;
    
    // read data waiting in buffer
    if (! PQconsumeInput(conn)) {
        fprintf(stderr, "Failed to consume pg input: %s\n",
            PQerrorMessage(conn));
        return;
    }
    
    // got query results?
    while (res = PQgetResult(conn)) {
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            fprintf(stderr, "Result error: %s", PQerrorMessage(conn));
            PQclear(res);
            return;
        }
        // memset(&opt, '\0', sizeof(opt));
        // PQprint(stdout, res, &opt);
    }
    
    // check for async notifs
    while (notify = PQnotifies(conn)) {
        printf("NOTIFY of '%s' received from backend PID %d: '%s'\n",
                notify->relname, notify->be_pid, notify->extra);
        PQfreemem(notify);
    }
}

