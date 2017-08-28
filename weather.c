#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "cJSON.h"



/*
void debug(const char* format,...){
	#ifdef DEBUG
	//printf(format,__VA_ARGS__);
	#endif
}
*/
//__VA_ARGS__只能用在宏的定义上面__VA_ARGS__ can only appear in the expansion of a C99 variadic macro
#ifdef DEBUG
#define debug(format, ...)  fprintf (stderr, format, __VA_ARGS__) 
#else
#define debug(format, ...)
#endif

void getIpByDomain(const char *url,char *ip_result,char *domain,char *resource){
	
	if(url != NULL){
		const char *start = url;
		char *has_protocal = strstr(url,"//");
		if(has_protocal != NULL){
			start = has_protocal + strlen("//");	
		}
		
		char *end = strstr(start,"/");
		if(end != NULL){
			strncpy(domain,start,end - start);
			strncpy(resource,end,strlen(end) + 1);
		}else{
			strcpy(domain,start);
		}
		
		
		debug("domain:%s,resource:%s\n",domain,resource);
		struct hostent *host = gethostbyname(domain);
		if(host != NULL){
			//int i = 0;
			for(int i = 0;host->h_addr_list[i];i++){
				char *ip = inet_ntoa(*((struct in_addr *)host->h_addr_list[i]));
				//strncpy(ip_result,ip,strlen(ip));需要bzero(ip_ret,32);
				
				snprintf(ip_result,strlen(ip) + 1,"%s",ip);//不需要bzero(ip_ret,32);注意长度需要加1，填充结束符
				//sprintf(ip_result,"%s",ip);
				break;
			}
		}
	}
}

#define MAX_BUFF 4096

void http_get_handle(char *read_data,int read_counts,int body_len,void *arg);

//get请求
char *getGetRequest(const char *ip,const char *get_request_header,
						void (*http_get_handle)(char *,int,int,void *),void *arg){
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd == -1)
	{
		perror("socket() error");
		exit(0);
	}
	
	int on = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	struct sockaddr_in srvaddr;
	bzero(&srvaddr, sizeof srvaddr);
	srvaddr.sin_family = AF_INET;
	inet_pton(AF_INET, ip, (struct sockaddr *)&srvaddr.sin_addr);
	srvaddr.sin_port = htons(80);
	
	int n = connect(sockfd, (struct sockaddr *)&srvaddr, sizeof srvaddr);
	if(n == -1)
	{
		perror("connect() failed");
		exit(0);
	}
	
	
	ssize_t retval = write(sockfd, get_request_header, strlen(get_request_header));
	if(retval == -1)
	{
		perror("write() error");
		exit(0);
	}

	char data_buf[MAX_BUFF];
	
	int read_counts;
	bool enter_body = false;
	int body_len = -1;
	char *body_data = NULL;
	int body_read = 0;
	int file_size = 0;
	while(1){
		bzero(data_buf,MAX_BUFF);
		
		read_counts = read(sockfd, data_buf, MAX_BUFF);
		debug("read_counts:%d\n",read_counts);
		if(read_counts == -1 && errno == EINTR){
			continue;
		}
		if(read_counts == -1)
		{
			perror("read() failed");
			exit(0);
		}else if(read_counts == 0){
			break;
		}
		
		if(body_len < 0){
			char *locate = strstr(data_buf,"Content-Length:");
			
			if(locate != NULL){
				body_len = atoi(locate + strlen("Content-Length:"));
				debug("body_len:%d\n",body_len);
				if(http_get_handle == NULL){
					body_data = malloc(body_len + 1);////最后一个字符为字符串结束符
					bzero(body_data,body_len);
				}
			}
			
			if(arg != NULL){
				FILE *file = (FILE *)arg;
				file_size = ftell(file) + body_len;
			}
			
		}
		
		if(!enter_body){
			char *locate = strstr(data_buf,"\r\n\r\n");
			if(locate != NULL){
				enter_body = true;
				
				char *read_start = locate + strlen("\r\n\r\n");
				int body_bytes = read_counts - (read_start - data_buf);
				if(http_get_handle != NULL){
					http_get_handle(read_start,body_bytes,file_size,arg);
				}else{
					//strcat(body_data,read_start);
					memcpy(body_data + 0,read_start,body_bytes);
					
				}
				
				body_read += body_bytes;
				
				//body_read += strlen(read_start);
				//debug("body_read:%d,read_start:%s",body_read,read_start);
			}
		}else{
			if(http_get_handle != NULL){
				http_get_handle(data_buf,read_counts,file_size,arg);
			}else{
				//strcat(body_data,data_buf);
				memcpy(body_data + body_read,data_buf,read_counts);
			}

			body_read += read_counts;
			//body_read += strlen(data_buf);
		}
		
		
		debug("body_read:%d,body_len:%d\n",body_read,body_len);
		if(body_read >= body_len){
			close(sockfd);
			sockfd = 0;
			break;
		}
		
		//debug("data_buf:%s,read_counts:%d\n",data_buf,read_counts);
		
		
	}
	
	
	if(body_data != NULL){
		//debug("body_data:%s\n",body_data);
		//debug("body_read:%d,body_len:%d\n",body_read,body_len);
		//debug("last letter:%c,%c\n",body_data[body_len-1],body_data[body_len]);
	}
	
	if(sockfd != 0){
		close(sockfd);
	}

	return body_data;
}

#define WEATHER_INFO_DAYS 3
#define WEATHER_INFO_LEN 20
typedef struct weather{
	char info[WEATHER_INFO_DAYS][WEATHER_INFO_LEN];//保存三天的天气情况
}weather;

bool parseJsonWeather(weather *weath,char *json_weather){
	cJSON *json = cJSON_Parse(json_weather);
	cJSON *status = cJSON_GetObjectItem(json, "status");
	/*
	printf("status->valueint:%d",status->valueint);
	printf("status->valuestring:%s",status->valuestring);
	if(status->valueint != 0){
		return false;
	}*/
	if(strcmp(status->valuestring,"0") != 0){
		return false;
	}
	cJSON *result = cJSON_GetObjectItem(json, "result");
	cJSON *daily = cJSON_GetObjectItem(result, "daily");
	if(cJSON_IsArray(daily)){
		int itemSize = cJSON_GetArraySize(daily);
		int i = 0;
		//debug("itemSize:%d\n",itemSize);
		for(;i < itemSize && i < WEATHER_INFO_DAYS;i++){//注意：数组的大小是WEATHER_INFO_DAYS，而itemSize为7
			cJSON *item = cJSON_GetArrayItem(daily,i);
			cJSON *day = cJSON_GetObjectItem(item, "day");
			cJSON *weather_json = cJSON_GetObjectItem(day, "weather");
			bzero(weath->info[i],WEATHER_INFO_LEN);
			strncpy(weath->info[i],weather_json->valuestring,WEATHER_INFO_LEN);
			//debug("weath->info[i]:%s\n",weath->info[i]);
		}
	}
	
	cJSON_Delete(json);
	
	return true;
}

void testWeather(){
	//http://www.bejson.com/jsonviewernew/
	/*  */
	char *url = "http://jisutqybmf.market.alicloudapi.com";
	char ip_ret[32];
	//bzero(ip_ret,32);
	char domain[128] = {0};
	char resource[128] = {0};
	getIpByDomain(url,ip_ret,domain,resource);
	
	
	//while(1){
		printf("输入城市名查询天气\n");
		//debug("ip_ret:%s\n",ip_ret);
		char city[20] = {0};
		fscanf(stdin,"%s",city);
		//debug("city:%s\n",city);
		
		char get_request[MAX_BUFF];
		memset(get_request,0,MAX_BUFF);
		snprintf(get_request,MAX_BUFF,"GET /weather/query?city=%s HTTP/1.1\r\n"
									"Host:%s\r\n"
									"Authorization:APPCODE 009d6c420c414873a84f050f27d26f0f\r\n\r\n",city,domain);
		//debug("get_request:%s\n",get_request);
		
		char *body = getGetRequest(ip_ret,get_request,NULL,NULL);
		//body = getGetRequest(ip_ret,get_request);
		//char *body = "{\"status\":\"0\",\"msg\":\"ok\",\"result\":{\"city\":\"广州\",\"cityid\":\"75\",\"citycode\":\"101280101\",\"date\":\"2017-08-26\",\"week\":\"星期六\",\"weather\":\"雷阵雨\",\"temp\":\"27\",\"temphigh\":\"34\",\"templow\":\"25\",\"img\":\"4\",\"humidity\":\"79\",\"pressure\":\"1007\",\"windspeed\":\"11.0\",\"winddirect\":\"北风\",\"windpower\":\"1级\",\"updatetime\":\"2017-08-26 08:19:14\",\"index\":[{\"iname\":\"空调指数\",\"ivalue\":\"部分时间开启\",\"detail\":\"您将感到些燥热，建议您在适当的时候开启制冷空调来降低温度，以免中暑。\"},{\"iname\":\"运动指数\",\"ivalue\":\"较不宜\",\"detail\":\"有降水，推荐您在室内进行低强度运动；若坚持户外运动，须注意选择避雨防滑并携带雨具。\"},{\"iname\":\"紫外线指数\",\"ivalue\":\"中等\",\"detail\":\"属中等强度紫外线辐射天气，外出时建议涂擦SPF高于15、PA+的防晒护肤品，戴帽子、太阳镜。\"},{\"iname\":\"感冒指数\",\"ivalue\":\"少发\",\"detail\":\"各项气象条件适宜，发生感冒机率较低。但请避免长期处于空调房间中，以防感冒。\"},{\"iname\":\"洗车指数\",\"ivalue\":\"不宜\",\"detail\":\"不宜洗车，未来24小时内有雨，如果在此期间洗车，雨水和路上的泥水可能会再次弄脏您的爱车。\"},{\"iname\":\"空气污染扩散指数\",\"index\":\"良\",\"detail\":\"气象条件有利于空气污染物稀释、扩散和清除，可在室外正常活动。\"},{\"iname\":\"穿衣指数\",\"ivalue\":\"炎热\",\"detail\":\"天气炎热，建议着短衫、短裙、短裤、薄型T恤衫等清凉夏季服装。\"}],\"aqi\":{\"so2\":\"9\",\"so224\":\"10\",\"no2\":\"40\",\"no224\":\"50\",\"co\":\"0.790\",\"co24\":\"0.840\",\"o3\":\"9\",\"o38\":\"6\",\"o324\":\"7\",\"pm10\":\"46\",\"pm1024\":\"48\",\"pm2_5\":\"29\",\"pm2_524\":\"25\",\"iso2\":\"4\",\"ino2\":\"20\",\"ico\":\"8\",\"io3\":\"4\",\"io38\":\"3\",\"ipm10\":\"41\",\"ipm2_5\":\"41\",\"aqi\":\"41\",\"primarypollutant\":\"PM10\",\"quality\":\"优\",\"timepoint\":\"2017-08-26 07:00:00\",\"aqiinfo\":{\"level\":\"一级\",\"color\":\"#00e400\",\"affect\":\"空气质量令人满意，基本无空气污染\",\"measure\":\"各类人群可正常活动\"}},\"daily\":[{\"date\":\"2017-08-26\",\"week\":\"星期六\",\"sunrise\":\"06:07\",\"sunset\":\"18:50\",\"night\":{\"weather\":\"雷阵雨\",\"templow\":\"25\",\"img\":\"4\",\"winddirect\":\"无持续风向\",\"windpower\":\"微风\"},\"day\":{\"weather\":\"雷阵雨\",\"temphigh\":\"34\",\"img\":\"4\",\"winddirect\":\"无持续风向\",\"windpower\":\"微风\"}},{\"date\":\"2017-08-27\",\"week\":\"星期日\",\"sunrise\":\"06:07\",\"sunset\":\"18:49\",\"night\":{\"weather\":\"中雨-大雨\",\"templow\":\"24\",\"img\":\"22\",\"winddirect\":\"东风\",\"windpower\":\"3-4 级\"},\"day\":{\"weather\":\"小雨-中雨\",\"temphigh\":\"31\",\"img\":\"21\",\"winddirect\":\"东风\",\"windpower\":\"4-5 级\"}},{\"date\":\"2017-08-28\",\"week\":\"星期一\",\"sunrise\":\"06:07\",\"sunset\":\"18:48\",\"night\":{\"weather\":\"小雨-中雨\",\"templow\":\"24\",\"img\":\"21\",\"winddirect\":\"无持续风向\",\"windpower\":\"微风\"},\"day\":{\"weather\":\"中雨-大雨\",\"temphigh\":\"31\",\"img\":\"22\",\"winddirect\":\"无持续风向\",\"windpower\":\"微风\"}},{\"date\":\"2017-08-29\",\"week\":\"星期二\",\"sunrise\":\"06:08\",\"sunset\":\"18:47\",\"night\":{\"weather\":\"雷阵雨\",\"templow\":\"26\",\"img\":\"4\",\"winddirect\":\"无持续风向\",\"windpower\":\"微风\"},\"day\":{\"weather\":\"雷阵雨\",\"temphigh\":\"33\",\"img\":\"4\",\"winddirect\":\"无持续风向\",\"windpower\":\"微风\"}},{\"date\":\"2017-08-30\",\"week\":\"星期三\",\"sunrise\":\"06:08\",\"sunset\":\"18:46\",\"night\":{\"weather\":\"雷阵雨\",\"templow\":\"26\",\"img\":\"4\",\"winddirect\":\"无持续风向\",\"windpower\":\"微风\"},\"day\":{\"weather\":\"雷阵雨\",\"temphigh\":\"35\",\"img\":\"4\",\"winddirect\":\"无持续风向\",\"windpower\":\"微风\"}},{\"date\":\"2017-08-31\",\"week\":\"星期四\",\"sunrise\":\"07:30\",\"sunset\":\"19:30\",\"night\":{\"weather\":\"雷阵雨\",\"templow\":\"26\",\"img\":\"4\",\"winddirect\":\"东北风\",\"windpower\":\"微风\"},\"day\":{\"weather\":\"雷阵雨\",\"temphigh\":\"34\",\"img\":\"4\",\"winddirect\":\"东北风\",\"windpower\":\"微风\"}},{\"date\":\"2017-09-01\",\"week\":\"星期五\",\"sunrise\":\"07:30\",\"sunset\":\"19:30\",\"night\":{\"weather\":\"雷阵雨\",\"templow\":\"26\",\"img\":\"4\",\"winddirect\":\"东北风\",\"windpower\":\"微风\"},\"day\":{\"weather\":\"雷阵雨\",\"temphigh\":\"34\",\"img\":\"4\",\"winddirect\":\"东北风\",\"windpower\":\"微风\"}}],\"hourly\":[{\"time\":\"9:00\",\"weather\":\"多云\",\"temp\":\"29\",\"img\":\"1\"},{\"time\":\"10:00\",\"weather\":\"多云\",\"temp\":\"31\",\"img\":\"1\"},{\"time\":\"11:00\",\"weather\":\"多云\",\"temp\":\"32\",\"img\":\"1\"},{\"time\":\"12:00\",\"weather\":\"晴\",\"temp\":\"33\",\"img\":\"0\"},{\"time\":\"13:00\",\"weather\":\"晴\",\"temp\":\"34\",\"img\":\"0\"},{\"time\":\"14:00\",\"weather\":\"晴\",\"temp\":\"34\",\"img\":\"0\"},{\"time\":\"15:00\",\"weather\":\"雷阵雨\",\"temp\":\"34\",\"img\":\"4\"},{\"time\":\"16:00\",\"weather\":\"雷阵雨\",\"temp\":\"33\",\"img\":\"4\"},{\"time\":\"17:00\",\"weather\":\"雷阵雨\",\"temp\":\"33\",\"img\":\"4\"},{\"time\":\"18:00\",\"weather\":\"雷阵雨\",\"temp\":\"32\",\"img\":\"4\"},{\"time\":\"19:00\",\"weather\":\"雷阵雨\",\"temp\":\"31\",\"img\":\"4\"},{\"time\":\"20:00\",\"weather\":\"多云\",\"temp\":\"31\",\"img\":\"1\"},{\"time\":\"21:00\",\"weather\":\"多云\",\"temp\":\"30\",\"img\":\"1\"},{\"time\":\"22:00\",\"weather\":\"晴\",\"temp\":\"30\",\"img\":\"0\"},{\"time\":\"23:00\",\"weather\":\"晴\",\"temp\":\"29\",\"img\":\"0\"},{\"time\":\"0:00\",\"weather\":\"多云\",\"temp\":\"29\",\"img\":\"1\"},{\"time\":\"1:00\",\"weather\":\"多云\",\"temp\":\"29\",\"img\":\"1\"},{\"time\":\"2:00\",\"weather\":\"多云\",\"temp\":\"29\",\"img\":\"1\"},{\"time\":\"3:00\",\"weather\":\"多云\",\"temp\":\"28\",\"img\":\"1\"},{\"time\":\"4:00\",\"weather\":\"多云\",\"temp\":\"28\",\"img\":\"1\"},{\"time\":\"5:00\",\"weather\":\"阵雨\",\"temp\":\"28\",\"img\":\"3\"},{\"time\":\"6:00\",\"weather\":\"阵雨\",\"temp\":\"27\",\"img\":\"3\"},{\"time\":\"7:00\",\"weather\":\"阵雨\",\"temp\":\"27\",\"img\":\"3\"},{\"time\":\"8:00\",\"weather\":\"阵雨\",\"temp\":\"28\",\"img\":\"3\"}]}}";
		if(body != NULL){
			//首先解析json封装成结构体
			//debug("sizeof(weather):%d\n",sizeof(weather));
			//weather *weath = (weather *)malloc(sizeof(weather));
			weather weath;
			bzero(&weath,sizeof(weather));
			
			bool parse_succeed = parseJsonWeather(&weath,body);
			if(!parse_succeed){
				printf("输入信息有误\n");
			}else{
				int i = 0;
				for(;i < WEATHER_INFO_DAYS ; i++){
					switch(i){
						case 0:
							printf("今天天气：");
							break;
						case 1:
							printf("明天天气：");
							break;
						case 2:
							printf("后天天气：");
							break;
					}
					printf("%s\n",weath.info[i]);
				}
			}
			
			//free(weath);
		}
		
		
		if(body != NULL){
			free(body);
		}
		
		
	//}
}

void download_file_handle(char *read_data,int read_counts,int file_size,void *arg){
	FILE *file = (FILE *)arg;
	fwrite(read_data,1,read_counts,file);//千万不能这样写，因为传递的字节流有可能就是'\0'结束符
	fflush(file);
	
	//fseek(file,0,SEEK_END);
	int persent = 100 * (ftell(file)/(float)(file_size));
	
	static bool output = false;
	if(output){
		printf("\r");
	}
	
	printf("%li---%d>>>>>>>>>下载进度>>>>>>>>>>>>>%d%%",
					ftell(file),file_size,persent);
	fflush(stdout);//必须加上，否则不会立即刷新输出，因为有缓冲区
	output = true;
    
}

void testDownload(){
	printf("输入你要下载的文件路径\n");
	char file_url[512] = {0};
	fscanf(stdin,"%s",file_url);
	//http://www.boa.org/boa-0.94.13.tar.gz
	
	int len = strlen(file_url);
	char *end = file_url + len - 1;
	char file_name[128] = {0};
	while(*end-- != '/' && end > file_url){
		if(*end == '/'){
			strcpy(file_name,end + 1);
			break;
		}
	}
	
	debug("file_name:%s\n",file_name);
	
	char ip_ret[32] = {0};
	char domain[128] = {0};
	char resource[128] = {0};
	getIpByDomain(file_url,ip_ret,domain,resource);
	
	FILE *file = fopen(file_name,"a+");
	int range = 0;
	if(file != NULL){
		fseek(file,0,SEEK_END);
		range = ftell(file);
	}
	
	char get_request[MAX_BUFF];
	memset(get_request,0,MAX_BUFF);
	snprintf(get_request,MAX_BUFF,"GET %s HTTP/1.1\r\n"
								"Host:%s\r\n"
								"Range:bytes=%d-\r\n\r\n",
								resource,domain,range);
	debug("get_request:%s\n",get_request);
	
	getGetRequest(ip_ret,get_request,download_file_handle,file);
	
	printf("\n下载完成\n");
	
	fclose(file);
}



int main(int argc ,char *argv[]){
	
	
	printf("输入你要进行的测试，1，天气\t2，断点续传\n");
	
	
	char input;
	while(1){
		fscanf(stdin,"%c",&input);
		switch(input){
			case '1':
				testWeather();
				break;
				
			case '2':
				testDownload();
				break;
		}
	}
	

	return 0;
}