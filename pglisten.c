#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

// channel to LISTEN on
const char *listenChannel = "person_updated";

void mainLoop(PGconn *conn);
void exitClean(PGconn *conn);
void handlePgRead(PGconn *conn);
void initListen(PGconn *conn);

int main() {
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
    PostgresPollingStatusType connStatus;
        
    while (! done) {        
        sock = PQsocket(conn);
        if (sock < 0) {
            printf("Postgres socket is gone\n");
            exitClean(conn);
        }
        
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        
        if (! connected) {
            connStatus = PQconnectPoll(conn);
            
            switch (connStatus) {
            case PGRES_POLLING_FAILED:
                fprintf(stderr, "Pg connection failed: %s",
                        PQerrorMessage(conn));
                return;
            case PGRES_POLLING_WRITING:
                FD_SET(sock, &wfds);
                break;
            case PGRES_POLLING_READING:
                FD_SET(sock, &rfds);
                break;
                
            case PGRES_POLLING_OK:
                printf("Connected\n");
                connected = 1;
                initListen(conn);
                break;
            }
        } 
        
        if (connected) {
            FD_SET(sock, &rfds);
        }
        
        retval = select(sock + 1, &rfds, &wfds, NULL, NULL);
        switch (retval) {
            case -1:
                perror("select() failed");
                done = 1;
                break;
            default:
                if (! connected)
                    break;

                if (FD_ISSET(sock, &rfds)) {
                    // ready to read from pg
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
    
    int qs = PQsendQuery(conn, query);
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

