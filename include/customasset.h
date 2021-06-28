#ifndef _CUSTOMASSET_H
#define _CUSTOMASSET_H
#include <config_category.h>
#include <string>
#include <logger.h>
#include <storage_client.h>

// #include <client_https.hpp>
 #include <client_http.hpp>
 using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

#define TO_STRING(...) DEFER(TO_STRING_)(__VA_ARGS__)
#define DEFER(x) x
#define TO_STRING_(...) #__VA_ARGS__
#define QUOTE(...) TO_STRING(__VA_ARGS__)




typedef void (*FuncPtr)(void *, void *);
/**
 * A simple customasset notification class that sends an customasset
 * via Fledge to the systems north of Fledge
 */
class CustomAsset {
	public:
		CustomAsset(ConfigCategory *config);
		~CustomAsset();

		void	notify(const std::string& notificationName, const std::string& triggerReason, const std::string& message);
		void	reconfigure(const std::string& newConfig);
		void	registerIngest(FuncPtr ingest, void *data)
		{
			m_ingest = ingest;
			m_data = data;
		};
	private:
		const std::string getAssetReading();
		void handleUnexpectedResponse(const char *operation, const std::string& responseCode,  const std::string& payload);

		//HttpClient *m_client;
		HttpClient *m_client;
		std::string	m_customasset;
		std::string	m_description;
		std::string	m_store;
		std::string	m_json_config;
		FuncPtr		m_ingest;
		void		*m_data;
};
#endif
