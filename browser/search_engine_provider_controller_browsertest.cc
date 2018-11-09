/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/strings/utf_string_conversions.h"
#include "brave/browser/profiles/brave_profile_manager.h"
#include "brave/browser/search_engine_provider_util.h"
#include "brave/browser/tor/tor_launcher_factory.h"
#include "brave/browser/ui/browser_commands.h"
#include "brave/components/search_engines/brave_prepopulated_engines.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "content/public/test/test_utils.h"

using SearchEngineProviderControllerTest = InProcessBrowserTest;

TemplateURLData CreateTestSearchEngine() {
  TemplateURLData result;
  result.SetShortName(base::ASCIIToUTF16("test1"));
  result.SetKeyword(base::ASCIIToUTF16("test.com"));
  result.SetURL("http://test.com/search?t={searchTerms}");
  return result;
}

// In Qwant region, alternative search engine prefs isn't used.
IN_PROC_BROWSER_TEST_F(SearchEngineProviderControllerTest,
                       PrivateWindowPrefTestWithNonQwantRegion) {
  Profile* profile = browser()->profile();
  Profile* incognito_profile = profile->GetOffTheRecordProfile();

  // This test case is only for non-qwant region.
  if (brave::IsRegionForQwant(profile))
    return;

  auto* service = TemplateURLServiceFactory::GetForProfile(profile);
  auto* incognito_service =
      TemplateURLServiceFactory::GetForProfile(incognito_profile);

  // Test pref is initially disabled.
  EXPECT_FALSE(brave::UseAlternativeSearchEngineProviderEnabled(profile));

  // Both mode should use same search engine if alternate pref is disabled.
  base::string16 normal_search_engine =
      service->GetDefaultSearchProvider()->data().short_name();
  EXPECT_EQ(service->GetDefaultSearchProvider()->data().short_name(),
            incognito_service->GetDefaultSearchProvider()->data().short_name());

  // Toggle pref and check incognito_service uses duckduckgo search engine and
  // normal mode service uses existing one.
  brave::ToggleUseAlternativeSearchEngineProvider(profile);
  EXPECT_TRUE(brave::UseAlternativeSearchEngineProviderEnabled(profile));
  EXPECT_EQ(incognito_service->GetDefaultSearchProvider()->data().short_name(),
            base::ASCIIToUTF16("DuckDuckGo"));
  EXPECT_EQ(service->GetDefaultSearchProvider()->data().short_name(),
            normal_search_engine);

  // Toggle pref again and check both mode uses same search engine.
  brave::ToggleUseAlternativeSearchEngineProvider(profile);
  EXPECT_FALSE(brave::UseAlternativeSearchEngineProviderEnabled(profile));
  EXPECT_EQ(service->GetDefaultSearchProvider()->data().short_name(),
            normal_search_engine);
  EXPECT_EQ(incognito_service->GetDefaultSearchProvider()->data().short_name(),
            normal_search_engine);

  // Check private search engine uses normal mode search engine when alternative
  // search engine pref is false.
  TemplateURLData test_data = CreateTestSearchEngine();
  std::unique_ptr<TemplateURL> test_url(new TemplateURL(test_data));
  service->SetUserSelectedDefaultSearchProvider(test_url.get());
  EXPECT_EQ(incognito_service->GetDefaultSearchProvider()->data().short_name(),
            base::ASCIIToUTF16("test1"));
}

// For qwant region, just check that both profile uses same provider.
IN_PROC_BROWSER_TEST_F(SearchEngineProviderControllerTest,
                       PrivateWindowTestWithQwantRegion) {
  Profile* profile = browser()->profile();
  Profile* incognito_profile = profile->GetOffTheRecordProfile();

  // This test case is only for qwant region.
  if (!brave::IsRegionForQwant(profile))
    return;

  auto* service = TemplateURLServiceFactory::GetForProfile(profile);
  auto* incognito_service =
      TemplateURLServiceFactory::GetForProfile(incognito_profile);

  // Test pref is initially disabled.
  EXPECT_FALSE(brave::UseAlternativeSearchEngineProviderEnabled(profile));

  // Toggling doesn't work in qwant region.
  brave::ToggleUseAlternativeSearchEngineProvider(profile);
  EXPECT_FALSE(brave::UseAlternativeSearchEngineProviderEnabled(profile));

  // Both mode should use same search engine.
  base::string16 normal_search_engine =
      service->GetDefaultSearchProvider()->data().short_name();
  EXPECT_EQ(service->GetDefaultSearchProvider()->data().short_name(),
            incognito_service->GetDefaultSearchProvider()->data().short_name());


  // Check private search engine uses normal mode search engine.
  TemplateURLData test_data = CreateTestSearchEngine();
  std::unique_ptr<TemplateURL> test_url(new TemplateURL(test_data));
  service->SetUserSelectedDefaultSearchProvider(test_url.get());
  EXPECT_EQ(incognito_service->GetDefaultSearchProvider()->data().short_name(),
            base::ASCIIToUTF16("test1"));
}

// Check crash isn't happened with multiple private window is used.
// https://github.com/brave/brave-browser/issues/1452
IN_PROC_BROWSER_TEST_F(SearchEngineProviderControllerTest,
                       MultiplePrivateWindowTest) {
  Browser* private_window_1 = CreateIncognitoBrowser();
  CloseBrowserSynchronously(private_window_1);

  Browser* private_window_2 = CreateIncognitoBrowser();
  brave::ToggleUseAlternativeSearchEngineProvider(private_window_2->profile());
}

// Checks the default search engine of the tor profile.
IN_PROC_BROWSER_TEST_F(SearchEngineProviderControllerTest,
                       PRE_CheckDefaultTorProfileSearchProviderTest) {
  ScopedTorLaunchPreventerForTest prevent_tor_process;

  brave::NewOffTheRecordWindowTor(browser());
  content::RunAllTasksUntilIdle();

  Profile* tor_profile = BrowserList::GetInstance()->GetLastActive()->profile();
  EXPECT_TRUE(tor_profile->IsTorProfile());

  auto* service = TemplateURLServiceFactory::GetForProfile(tor_profile);

  int default_provider_id = brave::IsRegionForQwant(tor_profile) ?
      TemplateURLPrepopulateData::PREPOPULATED_ENGINE_ID_QWANT :
      TemplateURLPrepopulateData::PREPOPULATED_ENGINE_ID_DUCKDUCKGO;

  //Check tor profile's search provider is set to ddg.
  EXPECT_EQ(service->GetDefaultSearchProvider()->data().prepopulate_id,
            default_provider_id);

  // Change provider to check whether it is retained in the next sessions.
  auto data = TemplateURLPrepopulateData::GetPrepopulatedEngine(
      tor_profile->GetPrefs(),
      TemplateURLPrepopulateData::PREPOPULATED_ENGINE_ID_BING);
  TemplateURL other_url(*data);
  service->SetUserSelectedDefaultSearchProvider(&other_url);
}

// Check changed provider in tor profile is retained across the sessions.
IN_PROC_BROWSER_TEST_F(SearchEngineProviderControllerTest,
                       CheckDefaultTorProfileSearchProviderTest) {
  ScopedTorLaunchPreventerForTest prevent_tor_process;

  brave::NewOffTheRecordWindowTor(browser());
  content::RunAllTasksUntilIdle();

  Profile* tor_profile = BrowserList::GetInstance()->GetLastActive()->profile();
  EXPECT_TRUE(tor_profile->IsTorProfile());

  int expected_provider_id =
      TemplateURLPrepopulateData::PREPOPULATED_ENGINE_ID_BING;
  auto* service = TemplateURLServiceFactory::GetForProfile(tor_profile);
  EXPECT_EQ(service->GetDefaultSearchProvider()->data().prepopulate_id,
            expected_provider_id);
}
