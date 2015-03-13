/*
 * Copyright (c) 2009-2013 Petri Lehtinen <petri@digip.org>
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <vector>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include <array>
#include <jansson.h>
#include <curl/curl.h>
#include <algorithm>
#include <iostream>
#include "i2c8Bit.h"
#include "mcp3008Spi.h"
#include <bitset>
#include <wiringPi.h>
#include <cmath>

#define BUFFER_SIZE  (256 * 1024)  /* 256 KB */
#define uint_64 unsigned long long
#define NUM_MCP23017 2
#define MAX_CIRCUITS 10

using namespace std;

/* Return the offset of the first newline in text or the length of
   text if there's no newline */

struct WriteThis {
  const char *readptr;
  long sizeleft;
};
 
static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
  struct WriteThis *pooh = (struct WriteThis *)userp;
 
  if(size*nmemb < 1)
    return 0;
 
  if(pooh->sizeleft) {
	*(char *)ptr = pooh->readptr[0]; /* copy one single byte */ 
    pooh->readptr++;                 /* advance pointer */ 
    pooh->sizeleft--;                /* less data left */ 
    return 1;                        /* we return 1 byte at a time! */ 
  }
 
  return 0;                          /* no more data left to deliver */ 
}

struct write_result
{
    char *data;
    int pos;
};

static size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream)
{
    struct write_result *result = (struct write_result *)stream;

    if(result->pos + size * nmemb >= BUFFER_SIZE - 1)
    {
        fprintf(stderr, "error: too small buffer\n");
        return 0;
    }

    memcpy(result->data + result->pos, ptr, size * nmemb);
    result->pos += size * nmemb;

    return size * nmemb;
}

static string request(const string url)
{
    CURL *curl = NULL;
    CURLcode status;
    char *data = NULL;
	char *urlchar;

	urlchar = new char[url.size() + 1];
	urlchar[url.size()] = 0;
	memcpy(urlchar, url.c_str(), url.size());

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    data = (char *)malloc(BUFFER_SIZE);

    struct write_result write_result = {
        .data = data,
        .pos = 0
    };

    curl_easy_setopt(curl, CURLOPT_URL, urlchar);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_result);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);

    status = curl_easy_perform(curl);
    if(status != CURLE_OK)
    {
        fprintf(stderr, "error: unable to request data from %s:\n", urlchar);
        fprintf(stderr, "%s\n", curl_easy_strerror(status));
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    data[write_result.pos] = '\0';

	string output = string(data);

	free(data);

    return output;
}

int send_data(string msrmt, const string url) {
	
	CURL *curl;
	CURLcode res;

	struct WriteThis pooh;
	char *urlchar;
	char *msrmtchar;

	msrmtchar = new char[msrmt.size() + 1];
	msrmtchar[msrmt.size()] = 0;
	memcpy(msrmtchar, msrmt.c_str(), msrmt.size());

	pooh.readptr = msrmtchar;
	pooh.sizeleft = (long)strlen(msrmtchar);

	urlchar = new char[url.size() + 1];
	urlchar[url.size()] = 0;
	memcpy(urlchar, url.c_str(), url.size());
	/* In windows, this will init the winsock stuff */ 
	res = curl_global_init(CURL_GLOBAL_DEFAULT);
	/* Check for errors */ 
	if(res != CURLE_OK) {
		fprintf(stderr, "curl_global_init() failed: %s\n",
		curl_easy_strerror(res));
		return 1;
	}

	/* get a curl handle */ 
	curl = curl_easy_init();
	if(curl) {
		/* First set the URL that is about to receive our POST. */ 
		curl_easy_setopt(curl, CURLOPT_URL, urlchar);

		/* Now specify we want to POST data */ 
		curl_easy_setopt(curl, CURLOPT_POST, 1L);

		/* we want to use our own read function */ 
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);

		/* pointer to pass to our read function */ 
		curl_easy_setopt(curl, CURLOPT_READDATA, &pooh);

		/* get verbose debug output please */ 

		/*
		If you use POST to a HTTP 1.1 server, you can send data without knowing
		the size before starting the POST if you use chunked encoding. You
		enable this by adding a header like "Transfer-Encoding: chunked" with
		CURLOPT_HTTPHEADER. With HTTP 1.0 or without chunked transfer, you must
		specify the size in the request.
		*/ 
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, pooh.sizeleft);
		/* use curl_slist_free_all() after the *perform() call to free this
		list again */ 

		/* Perform the request, res will get the return code */ 
		res = curl_easy_perform(curl);
		/* Check for errors */ 
		if(res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n",

		curl_easy_strerror(res));

		/* always cleanup */ 
		curl_easy_cleanup(curl);
	}
	curl_global_cleanup();
	return 0;	
}

void chip_select(int chip)
{
    switch (chip)
    {
        case 0:
            digitalWrite(6, 0); 
            digitalWrite(5, 1);
            break;
        case 1:
            digitalWrite(6, 1);
            digitalWrite(5, 0);
            break;
        default:
            printf("unrecognied chip number\n"); 
            break;
    }
}

double v_to_p (int v)
{
    double power = 0;
    double adc_voltage = (5.0/1024.0 * (double)v);
    double measured_current = abs(2.5 - adc_voltage) * 66000;     
    
    power = measured_current * 120.0;

    return power;
}

vector<double> read_circuits(mcp3008Spi &a2d, int numcircuit) {
    
	int a2dVal = 0;
    int a2dChannel = 0;
    unsigned char data[3];
	vector<double> measurements;
	unsigned char channel = 0b10000000;	
    int chip_counter = 0;

	for (int i = 0; i < numcircuit ;i++) {
		chip_counter = i/8;
        printf("chip counter %d\n", chip_counter);    
        chip_select(chip_counter); 
        
        data[0] = 1;  //  first byte transmitted -> start bit
		data[1] = channel |( ((a2dChannel & 7) << 4)); // second byte transmitted -> (SGL/DIF = 1, D2=D1=D0=0)
		data[2] = 0; // third byte transmitted....don't care

        a2d.spiWriteRead(data, sizeof(data) );
	
      	a2dVal = 0;
		a2dVal = (data[1]<< 8) & 0b1100000000; //merge data[1] & data[2] to get result
		a2dVal |=  (data[2] & 0xff);
        
        if (chip_counter == 0 && channel == 0b10000000)
            measurements.push_back(v_to_p(a2dVal)); 
        else 
		    measurements.push_back(a2dVal);
	
    	cout << "The Result on channel " << i << " is: " << a2dVal << endl;
		channel = channel + (1 << 4);

	
	}	
/*
	data[0] = 1;  //  first byte transmitted -> start bit
	data[1] = 0b10010000 |( ((a2dChannel & 7) << 4)); // second byte transmitted -> (SGL/DIF = 1, D2=D1=D0=0)
	data[2] = 0; // third byte transmitted....don't care

	a2d.spiWriteRead(data, sizeof(data) );

	a2dVal = 0;
	a2dVal = (data[1]<< 8) & 0b1100000000; //merge data[1] & data[2] to get result
	a2dVal |=  (data[2] & 0xff);
	cout << "The Result on channel 1 is: " << a2dVal << endl;
 */ 
	return measurements;
}

int toggle_circuits(uint_64 togglestate, i2c8Bit &mcp23017){
    togglestate = togglestate ^ 1;
	
    printf("LED state is: %lld\n", togglestate);

    mcp23017.writeAllReg(0x14,togglestate);

	return 0;
}

string getJsonFromArray(string rpi_id, vector<int> circuits, vector<double> values){
    string serial = "\"serial\": \"" + rpi_id + "\",";
    string readings = "\"readings\":[";
	int length = values.size();
	for (int i = 0; i < length; i++){
        if (i != 0) readings += ",";
        readings += "{\"power\":" + to_string(values[i]) + ",\"circuit_num\":" + to_string(circuits[i]) + "}";
    }
    readings += "]";
    return ("{" + serial + readings + "}");
}

uint_64 parseJSON(string curldata, vector<int> *circuits) {
    size_t i;
	uint_64 bitint = 0; 
    json_t *root;
    json_t *data;
 
    json_error_t error;

	char *curldatachar;

	curldatachar = new char[curldata.size() + 1];
	curldatachar[curldata.size()] = 0;
	memcpy(curldatachar, curldata.c_str(), curldata.size());
	root = json_loads(curldatachar, 0, &error); 	
    int circuit_counter = 0;
 
	if(!root)
	{
	    fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
	    return 1;
	}
 
	data = json_object_get(root, "data");
 
	if(!json_is_array(data))
	{
	    fprintf(stderr, "error: root is not an array\n");
	    json_decref(data);
	    return 1;
	}

	unsigned int jsonarraysize = static_cast<unsigned int>(json_array_size(data));
	size_t array_size = (jsonarraysize);

    printf("array_size is %d\n", jsonarraysize);

    if(circuits->size() == 0)
    {
        for(i = 0; i < array_size; i++)
        {	
            json_t *arr_data, *c_id, *state;
             
            arr_data = json_array_get(data, i);
            c_id = json_object_get(arr_data, "circuit_num");
            state = json_object_get(arr_data, "state");
            circuits->push_back(json_integer_value(c_id));
            cout << circuit_counter << endl;
            cout << json_is_true(state) << endl;
            bitint = bitint + (json_is_true(state) << (circuit_counter));
            circuit_counter++;
        }
    } 
    else
    {
        for(i = 0; i < array_size; i++)
        {	
            json_t *arr_data, *state;
             
            arr_data = json_array_get(data, i);
            state = json_object_get(arr_data, "state");
            cout << circuit_counter << endl;
            cout << json_is_true(state) << endl;
            bitint = bitint + (json_is_true(state) << (circuit_counter));
            circuit_counter++;
        }
    }	

	return bitint;

}

int main(void)
{
    string text;
    string url;
	string posturl;

    vector<int> circuits;   
	vector<double> testoutput;
   	string rpid = "rpijavier";
	string cmposedResult;
    vector<unsigned char> dev_addrs;
    //vector<mcp3008Spi> a2d_list;
    dev_addrs.push_back(0b00100000);
    dev_addrs.push_back(0b00100001);

    mcp3008Spi a2d("/dev/spidev0.0", SPI_MODE_0, 1000000, 8);
	i2c8Bit mcp23017(dev_addrs,string("/dev/i2c-1"), NUM_MCP23017);
    mcp23017.writeAllReg(0b00,0b00000000);	
  
	//url = "http://pb-elite.website/api/testResponse/1/";
	//posturl = "http://pb-elite.website/api/sendReading/";

	url = "http://192.168.43.253:8000/api/testResponse/1/";
	posturl = "http://192.168.43.253:8000/api/sendReading/";
    wiringPiSetup();

    for (int pin = 4; pin < 7; ++pin)
    {
        pinMode(pin, OUTPUT);
    }

    digitalWrite(4, 1);
    digitalWrite(5, 1);
    digitalWrite(6, 0);

	//cout << getJsonFromArray(rpid, test) << endl;

	while (1) {

		text = request(url);

		printf("%s\n", text.c_str());
	
        printf("printed curl request\n");
    
        if (text.empty())
        {  
            printf("cannot connect to server, opening all circuits\n"); 
            uint_64 input = 0;
            for (int i = 0; i < MAX_CIRCUITS; i++)
            {
                input = input + (0 << (i));
            }
            toggle_circuits(input, mcp23017); 
        }
        else
        {	
		    toggle_circuits(parseJSON(text, &circuits), mcp23017);
		
            sleep(1);

            testoutput = read_circuits(a2d, circuits.size()); 

            cmposedResult = getJsonFromArray(rpid, circuits, testoutput);

            printf("%s\n", cmposedResult.c_str());

            send_data(cmposedResult, posturl);
        }
    
	
		sleep(1);
	}
    return 0;
}
