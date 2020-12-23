#ifndef WABBITHTTP_H
#define WABBITHTTP_H

/*
 *  Reads the file pointed to by a url into a buffer
 *
 * url: url of the file to read
 * buffer: pointer to the buffer to read the data into
 * buffer_size: size of the buffer passed in, in bytes
 * callback: callback to s
 *
 * returns: pointer to buffer, or NULL if an error occurred
 */
size_t ReadFileHttp(TCHAR *url, LPVOID buffer, size_t buffer_size, INTERNET_STATUS_CALLBACK callback);

#endif