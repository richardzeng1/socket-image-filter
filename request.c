#include "request.h"
#include "response.h"
#include <string.h>


/******************************************************************************
 * ClientState-processing functions
 *****************************************************************************/
ClientState *init_clients(int n) {
    ClientState *clients = malloc(sizeof(ClientState) * n);
    for (int i = 0; i < n; i++) {
        clients[i].sock = -1;  // -1 here indicates available entry
    }
    return clients;
}

/*
 * Remove the client from the client array, free any memory allocated for
 * fields of the ClientState struct, and close the socket.
 */
void remove_client(ClientState *cs) {
    if (cs->reqData != NULL) {
        free(cs->reqData->method);
        free(cs->reqData->path);
        for (int i = 0; i < MAX_QUERY_PARAMS && cs->reqData->params[i].name != NULL; i++) {
            free(cs->reqData->params[i].name);
            free(cs->reqData->params[i].value);
        }
        free(cs->reqData);
        cs->reqData = NULL;
    }
    close(cs->sock);
    cs->sock = -1;
    cs->num_bytes = 0;
}


/*
 * Search the first inbuf characters of buf for a network newline ("\r\n").
 * Return the index *immediately after* the location of the '\n'
 * if the network newline is found, or -1 otherwise.
 * Definitely do not use strchr or any other string function in here. (Why not?)
 */
int find_network_newline(const char *buf, int inbuf) {

    int end_start_line = -1;

    for (int index = 0; index<inbuf; index++){
        if (buf[index] == '\r'){
            if (buf[index+1] == '\n'){
                end_start_line = index+2;
                break;
            }
        }
    }

    return end_start_line;
}

/*
 * Removes one line (terminated by \r\n) from the client's buffer.
 * Update client->num_bytes accordingly.
 *
 * For example, if `client->buf` contains the string "hello\r\ngoodbye\r\nblah",
 * after calling remove_line on it, buf should contain "goodbye\r\nblah"
 * Remember that the client buffer is *not* null-terminated automatically.
 */
void remove_buffered_line(ClientState *client) {
    int second_line_index = find_network_newline(client->buf, client->num_bytes);
    // second_line_index will always be equal to or greater than 2 if there is
    // a new line meaning if second_line_index!=-1
    if (second_line_index!=-1){
        int x = 0;
        for (int index = second_line_index; index < client->num_bytes; index++){
            client->buf[x]= client->buf[index];
            x++;
        }
        client->num_bytes-=second_line_index;
        client->buf[client->num_bytes]='\0';

    }
}


/*
 * Read some data into the client buffer. Append new data to data already
 * in the buffer.  Update client->num_bytes accordingly.
 * Return the number of bytes read in, or -1 if the read failed.

 * Be very careful with memory here: there might be existing data in the buffer
 * that you don't want to overwrite, and you also don't want to go past
 * the end of the buffer, and you should ensure the string is null-terminated.
 */
int read_from_client(ClientState *client) {
    int read_result = read(client->sock, &client->buf[client->num_bytes], MAXLINE-1-client->num_bytes);
    if (read_result <= -1){
        return -1;
    }else{
        client->num_bytes+=read_result;
        client->buf[client->num_bytes] = '\0';
        return read_result;
    }
    return -1;
}


/*****************************************************************************
 * Parsing the start line of an HTTP request.
 ****************************************************************************/
// Helper function declarations.
void parse_query(ReqData *req, const char *str);
void update_fdata(Fdata *f, const char *str);
void fdata_free(Fdata *f);
void log_request(const ReqData *req);


/* If there is a full line (terminated by a network newline (CRLF))
 * then use this line to initialize client->reqData
 * Return 0 if a full line has not been read, 1 otherwise.
 */
int parse_req_start_line(ClientState *client) {
    int end_start_line = find_network_newline(client->buf, client->num_bytes);
    if (end_start_line<0){
        return 0;
    }
    ReqData *start_line = malloc(sizeof(ReqData));

    char first_line[end_start_line+1];
    char *pass_line = malloc(sizeof(char)*(end_start_line+1));
    // line to operate on
    strncpy(first_line, client->buf, end_start_line);
    // line to pass
    strncpy(pass_line, client->buf, end_start_line);
    first_line[end_start_line]='\0';
    pass_line[end_start_line]='\0';

    char *target;
    char *method;

    // Generating method (POST or GET)
    method = strtok(first_line, " ");
    char *struct_method = malloc(sizeof(char)*(strlen(method)+1));
    memcpy(struct_method, method, strlen(method));
    struct_method[strlen(method)]='\0';
    start_line->method = struct_method;

    if (strstr(pass_line, "?")==NULL){
        target = strtok(NULL, " ");
    }else{
        target = strtok(NULL, "?");
    }

    // Generating path
    char *struct_target = malloc(sizeof(char)*(strlen(target)+1));
    memcpy(struct_target, target, strlen(target));
    struct_target[strlen(target)]='\0';
    start_line->path = struct_target;

    for (int index =0 ;index<MAX_QUERY_PARAMS; index++){
        start_line->params[index].name = NULL;
        start_line->params[index].value = NULL;
    }
    // Parsing queries if ? is present
    if (strstr(pass_line, "?")!=NULL){
        parse_query(start_line, pass_line);
    }
    client->reqData = start_line;
    free(pass_line);
    // This part is just for debugging purposes.
    log_request(start_line);
    return 1;
}


/*
 * Initializes req->params from the key-value pairs contained in the given
 * string.
 * Assumes that the string is the part after the '?' in the HTTP request target,
 * e.g., name1=value1&name2=value2.
 */
void parse_query(ReqData *req, const char *str) {

    //char *str_copy=malloc(sizeof(char)*MAXLINE);
    char str_copy[sizeof(char)*MAXLINE];
    strncpy(str_copy, str, strlen(str));
    strtok(str_copy, " ");
    strtok(NULL, "?");

    // Getting pairs of queries
    char *queries = strtok(NULL, " ");
    int counter = 0;
    char **pairs=malloc(sizeof(char*)*MAX_QUERY_PARAMS);
    char *query_result = strtok(queries, "&");
    while(query_result!=NULL){
        pairs[counter] = query_result;
        query_result = strtok(NULL, "&");
        counter++;
    }

    // Generating name and values for pairs
    for (int index = 0; index < counter; index++){
        char *name = strtok(pairs[index], "=");
        char *value =  strtok(NULL, "=");

        char *param_name = malloc(sizeof(char)*(strlen(name)+1));
        char *param_value = malloc(sizeof(char)*(strlen(value)+1));
        memcpy(param_name, name, strlen(name));
        memcpy(param_value, value, strlen(value));
        param_name[strlen(name)]='\0';
        param_value[strlen(value)]='\0';

        req->params[index].value =param_value;
        req->params[index].name = param_name;
    }
}




/*
 * Print information stored in the given request data to stderr.
 */
void log_request(const ReqData *req) {
    fprintf(stderr, "Request parsed: [%s] [%s]\n", req->method, req->path);
    for (int i = 0; i < MAX_QUERY_PARAMS && req->params[i].name != NULL; i++) {
        fprintf(stderr, "  %s -> %s\n",
                req->params[i].name, req->params[i].value);
    }
}


/******************************************************************************
 * Parsing multipart form data (image-upload)
 *****************************************************************************/

char *get_boundary(ClientState *client) {
    int len_header = strlen(POST_BOUNDARY_HEADER);

    while (1) {
        int where = find_network_newline(client->buf, client->num_bytes);
        if (where > 0) {
            if (where < len_header || strncmp(POST_BOUNDARY_HEADER, client->buf, len_header) != 0) {
                remove_buffered_line(client);
            } else {
                // We've found the boundary string!
                // We are going to add "--" to the beginning to make it easier
                // to match the boundary line later
                char *boundary = malloc(where - len_header + 1);
                strncpy(boundary, "--", where - len_header + 1);
                strncat(boundary, client->buf + len_header, where - len_header - 1);
                boundary[where - len_header] = '\0';
                return boundary;
            }
        } else {
            // Need to read more bytes
            if (read_from_client(client) <= 0) {
                // Couldn't read; this is a bad request, so give up.
                return NULL;
            }
        }
    }
    return NULL;
}


char *get_bitmap_filename(ClientState *client, const char *boundary) {
    int len_boundary = strlen(boundary);

    // Read until finding the boundary string.
    while (1) {
        int where = find_network_newline(client->buf, client->num_bytes);
        if (where > 0) {
            if (where < len_boundary + 2 ||
                    strncmp(boundary, client->buf, len_boundary) != 0) {
                remove_buffered_line(client);
            } else {
                // We've found the line with the boundary!
                remove_buffered_line(client);
                break;
            }
        } else {
            // Need to read more bytes
            if (read_from_client(client) <= 0) {
                // Couldn't read; this is a bad request, so give up.
                return NULL;
            }
        }
    }

    int where = find_network_newline(client->buf, client->num_bytes);

    client->buf[where-1] = '\0';  // Used for strrchr to work on just the single line.
    char *raw_filename = strrchr(client->buf, '=') + 2;
    int len_filename = client->buf + where - 3 - raw_filename;
    char *filename = malloc(len_filename + 1);
    strncpy(filename, raw_filename, len_filename);
    filename[len_filename] = '\0';

    // Restore client->buf
    client->buf[where - 1] = '\n';
    remove_buffered_line(client);
    return filename;
}

/*
 * Read the file data from the socket and write it to the file descriptor
 * file_fd.
 * You know when you have reached the end of the file in one of two ways:
 *    - search for the boundary string in each chunk of data read
 * (Remember the "\r\n" that comes before the boundary string, and the
 * "--\r\n" that comes after.)
 *    - extract the file size from the bitmap data, and use that to determine
 * how many bytes to read from the socket and write to the file
 */
int save_file_upload(ClientState *client, const char *boundary, int file_fd) {
    // Read in the next two lines: Content-Type line, and empty line
    remove_buffered_line(client);
    remove_buffered_line(client);

    int error;
    int bytes_written=0;

    // Getting size of file
    int size;
    memcpy(&size, &(client->buf[2]), sizeof(int));
    while(bytes_written<size){
        read_from_client(client);
        // Writing last of data
        if (size-bytes_written-client->num_bytes<=0){
            error = write(file_fd, client->buf, size-bytes_written);
            if (error!=size-bytes_written){
                perror("write");
                exit(1);
            }
            bytes_written+=size-bytes_written;
        }else{
            error = write(file_fd, client->buf, client->num_bytes);
            if (error!=client->num_bytes){
                perror("write");
                exit(1);
            }
            bytes_written+=client->num_bytes;
        }
        if (client->num_bytes==MAXLINE-1){
            client->num_bytes=0;
        }
    }
    return bytes_written;
}
