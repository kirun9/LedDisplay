#ifndef _REQUESTPARSER_h
#define _REQUESTPARSER_h

#include "arduino.h"

enum Method {
    UNKNOWN = 0,
    GET,
    POST,
    HEAD
};

class Request {
public:
    String path;
    Method method;
    String host;
};

class RequestParser {
private:
    int index;

    String parseToken(String& request, char delimiter) {
        size_t startPos = index;
        while (index < request.length() && request[index] != delimiter) {
            index++;
        }
        return request.substring(startPos, index);
    }
public:
    Request parse(String request) {
        Request parsedRequest;

        // Parse method
        parsedRequest.method = Method::UNKNOWN;
        if (request.indexOf("GET") == 0) {
            parsedRequest.method = Method::GET;
        }
        else if (request.indexOf("POST") == 0) {
            parsedRequest.method = Method::POST;
        }
        else if (request.indexOf("HEAD") == 0) {
            parsedRequest.method = Method::HEAD;
        }

        // Parse path
        index = request.indexOf(" ") + 1;
        parsedRequest.path = parseToken(request, ' ');

        // Parse host
        index = request.indexOf("Host:") + 6;  // Move index to start of host
        while (index < request.length() && request[index] == ' ') {
            index++;
        }
        parsedRequest.host = parseToken(request, '\r');

        return parsedRequest;
    }
};
#endif