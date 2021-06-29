/*
 * Fledge "customasset" notification delivery plugin.
 *
 * Copyright (c) 2019 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include "customasset.h"
#include <logger.h>
#include <reading.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <string_utils.h>
#include <sstream>
#include <iomanip>


using namespace std;
using namespace rapidjson;

const char * sample_store = QUOTE({
	"sinusoid" : {
		"sinusoid" : 0.5,
		"timestamp" : "2019-01-01 10:00:00.123456+00:00"
	},
	"opcuajob" : {
		"job" : "P60032085",
		"timestamp" : "2019-01-01 10:00:00.123456+00:00"
	},
	"opcuaproductid" : {
		"productid" : "S6866",
		"timestamp" : "2019-01-01 10:00:00.123456+00:00"
	}
});


/**
 * Construct a customasset notification plugin
 *
 * @param category	The configuration of the plugin
 */
CustomAsset::CustomAsset(ConfigCategory *category)
{
	m_customasset = category->getValue("customasset");
	m_description = category->getValue("description");
	m_json_config = category->getValue("jsonconfig");
	// TODO We have no access to the storage layer
	m_ingest = NULL;

	//Get assetNames
	assetNames = getAssetNamesConfig();

	try{
			m_client = new HttpClient("localhost:8081");
			auto res = m_client->request("GET", "/fledge/audit?limit=1");
	} catch (exception& ex) {
			Logger::getLogger()->error("Failed to connect to server: %s", ex.what());
			throw;
	}
}

/**
 * The destructure for the customasset plugin
 */
CustomAsset::~CustomAsset()
{
	delete m_client;
	m_client = NULL;
}

/**
 * Send a notification via the CustomAsset by ingesting into the Fledge storage layer
 *
 * @param notificationName 	The name of this notification
 * @param triggerReason		Why the notification is being sent
 * @param message		The message to send
 */
void CustomAsset::notify(const string& notificationName, const string& triggerReason, const string& message)
{
	vector<Datapoint *>	datapoints;

	if (!m_ingest)
	{
		return;
	}

	DatapointValue dpv1(m_description);
	datapoints.push_back(new Datapoint("description", dpv1));

	Document doc;
	doc.Parse(triggerReason.c_str());
	if (!doc.HasParseError())
	{
		if (doc.HasMember("reason"))
		{
			if (doc["reason"].IsString())
			{
				DatapointValue dpv3(doc["reason"].GetString());
				datapoints.push_back(new Datapoint("event", dpv3));
			}
			else if (doc["reason"].IsInt64())
			{
				DatapointValue dpv3((long) doc["reason"].GetInt64());
				datapoints.push_back(new Datapoint("event", dpv3));
			}
			else
			{
				Logger::getLogger()->error("The reason returned from the rule for delivery is of a bad type");
			}
		}
	}
	DatapointValue dpv4(notificationName);
	datapoints.push_back(new Datapoint("rule", dpv4));

	string readings;
	rapidjson::Document tempDoc;
	rapidjson::Document jsonDoc;
	jsonDoc.SetObject();
	rapidjson::Value tempReadingArray(rapidjson::kArrayType);
	rapidjson::Document::AllocatorType& allocator = jsonDoc.GetAllocator();
	for(std::size_t i = 0; i < assetNames.size(); ++i) {
    readings = getAssetReading(assetNames[i]);
		tempDoc.Parse<0>(readings.c_str());
		if(tempDoc.IsArray()){
			if(tempDoc.Size()==1){
				if(tempDoc[0].IsObject()){
						tempReadingArray.PushBack(tempDoc[0], allocator);
				}
			}else{
				for(SizeType i = 0; i < tempDoc.Size(); i++){
					tempReadingArray.PushBack(tempDoc[i], allocator);
				}
			}
		}
	}

	jsonDoc.AddMember("readings", tempReadingArray, allocator);
	rapidjson::StringBuffer strbuf;
	rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
	jsonDoc.Accept(writer);

	const char *jsonString = strbuf.GetString();

	m_store = escape_json(jsonString);

	//m_store = ss.str();

	DatapointValue dpv5(m_store);
	datapoints.push_back(new Datapoint("store", dpv5));

	Reading customasset(m_customasset, datapoints);

	(*m_ingest)(m_data, &customasset);
}

/**
 * Reconfigure the customasset delivery plugin
 *
 * @param newConfig	The new configuration
 */
void CustomAsset::reconfigure(const string& newConfig)
{
	ConfigCategory category("new", newConfig);
	m_customasset = category.getValue("customasset");
	m_description = category.getValue("description");
	m_json_config = category.getValue("jsonconfig");
	assetNames = getAssetNamesConfig();
}



const std::string CustomAsset::getAssetReading(const std::string& assetName)
{
	try {
			//char url[256];
			//Logger::getLogger()->warn("Hello getAssetReading");
			//snprintf(url, sizeof(url), "/fledge/asset/%s?limit=1", "opcuajob");
			//Logger::getLogger()->warn("url: %s", url);
			auto res = m_client->request("GET", "/fledge/asset/" + urlEncode(assetName) + "?limit=1");
			if (res->status_code.compare("200 OK") == 0)
			{
				ostringstream resultPayload;
				resultPayload << res->content.rdbuf();
				std::string result = resultPayload.str();
				return result;
			}
			ostringstream resultPayload;
			resultPayload << res->content.rdbuf();
			handleUnexpectedResponse("Fetch readings", res->status_code, resultPayload.str());
	} catch (exception& ex) {
			Logger::getLogger()->error("Failed to fetch asset: %s", ex.what());
			throw;
	}
}

/**
 * Standard logging method for all interactions
 *
 * @param operation	The operation beign undertaken
 * @param responseCode	The HTTP response code
 * @param payload	The payload in the response message
 */
void CustomAsset::handleUnexpectedResponse(const char *operation, const std::string& responseCode,  const std::string& payload)
{
	rapidjson::Document doc;

	doc.Parse(payload.c_str());
	if (!doc.HasParseError())
	{
		if (doc.HasMember("message"))
		{
			Logger::getLogger()->info("%s completed with result %s", operation, responseCode.c_str());
			Logger::getLogger()->error("%s: %s", operation, doc["message"].GetString());
		}
	}
	else
	{
		Logger::getLogger()->error("%s completed with result %s", operation, responseCode.c_str());
	}
}

const std::string CustomAsset::getUtcDateTimeNow()
{
	struct timeval now;
	gettimeofday(&now, NULL);

	//return getUtcDateTime(now);

	#define DATE_TIME_BUFFER_LEN 52
	char date_time[DATE_TIME_BUFFER_LEN];
	char microsec[10];

	struct tm utcnow;
	gmtime_r(&now.tv_sec, &utcnow);

	std::strftime(date_time, sizeof(date_time), "%Y-%m-%d %H:%M:%S", &utcnow);
	snprintf(microsec, sizeof(microsec), ".%06lu", now.tv_usec);

	ostringstream ss;
	ss << date_time << microsec << "+00:00";

	return ss.str();
}

const std::string CustomAsset::getUtcDateTime(struct timeval value)
{

	#define DATE_TIME_BUFFER_LEN 52
	char date_time[DATE_TIME_BUFFER_LEN];
	char microsec[10];

	struct tm utcnow;
	gmtime_r(&value.tv_sec, &utcnow);

	std::strftime(date_time, sizeof(date_time), "%Y-%m-%d %H:%M:%S", &utcnow);
	snprintf(microsec, sizeof(microsec), ".%06lu", value.tv_usec);

	ostringstream ss;
	ss << date_time << microsec << "+00:00";

	return ss.str();
}

std::string CustomAsset::escape_json(const std::string &s)
{
	std::ostringstream o;
	for (auto c = s.cbegin(); c != s.cend(); c++)
	{
		switch (*c)
		{
			case '"': o << "\\\""; break;
			case '\\': o << "\\\\"; break;
			case '\b': o << "\\b"; break;
			case '\f': o << "\\f"; break;
			case '\n': o << "\\n"; break;
			case '\r': o << "\\r"; break;
			case '\t': o << "\\t"; break;
			default:
				if ('\x00' <= *c && *c <= '\x1f') {
					o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)*c;
				}
				else
				{
					o << *c;
				}
		}
	}
	return o.str();
}

const std::vector<std::string> CustomAsset::getAssetNamesConfig()
{
	std::vector<std::string> assetNames;
	Document configDoc;
	configDoc.Parse(m_json_config.c_str());

	for (Value::ConstMemberIterator itr = 	configDoc.MemberBegin();itr != 	configDoc.MemberEnd(); ++itr)
  {
		assetNames.push_back(itr->name.GetString());
 	}
	return assetNames;
}

const std::vector<std::string> CustomAsset::getAssetDatapointsConfig(const std::string &assetName)
{
	std::vector<std::string> datapointNames;
	Document configDoc;
	configDoc.Parse(m_json_config.c_str());
	if(configDoc.HasMember(assetName.c_str()))
	{
		const Value& data = configDoc[assetName.c_str()];
		for (SizeType i = 0; i < data.Size(); i++) // Uses SizeType instead of size_t
		{
      datapointNames.push_back(data[i].GetString());
		}
	}else{
		return {};
	}
	return datapointNames;
}
