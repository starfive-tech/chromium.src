// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_destroyer.h"

#include <vector>
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileDestroyerTest : public testing::Test,
                             public testing::WithParamInterface<bool> {
 public:
  ProfileDestroyerTest() = default;
  ProfileDestroyerTest(const ProfileDestroyerTest&) = delete;
  ProfileDestroyerTest& operator=(const ProfileDestroyerTest&) = delete;

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  TestingProfile* original_profile() { return original_profile_; }
  TestingProfile* OtrProfile(size_t i) {
    return otr_profiles_.size() > i ? otr_profiles_[i] : nullptr;
  }

  void CreateOriginalProfile() {
    original_profile_ = profile_manager_.CreateTestingProfile("foo");
    original_profile_->SetProfileDestructionObserver(
        base::BindOnce(&ProfileDestroyerTest::SetOriginalProfileDestroyed,
                       base::Unretained(this)));
    original_profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
        original_profile_, ProfileKeepAliveOrigin::kBrowserWindow);
  }

  void CreateOTRProfile() {
    Profile::OTRProfileID profile_id =
        (is_primary_otr_ && otr_profiles_.size() == 0)
            ? Profile::OTRProfileID::PrimaryID()
            : Profile::OTRProfileID::CreateUniqueForTesting();

    TestingProfile::Builder builder;
    builder.SetPath(original_profile_->GetPath());
    TestingProfile* otr_profile =
        builder.BuildOffTheRecord(original_profile_, profile_id);
    otr_profile->SetProfileDestructionObserver(
        base::BindOnce(&ProfileDestroyerTest::SetOTRProfileDestroyed,
                       base::Unretained(this), base::Unretained(otr_profile)));
    otr_profiles_.push_back(otr_profile);
  }

  void SetOriginalProfileDestroyed() { original_profile_ = nullptr; }
  void SetOTRProfileDestroyed(TestingProfile* destroyed_profile) {
    for (auto& profile : otr_profiles_) {
      if (profile == destroyed_profile) {
        profile = nullptr;
      }
    }
  }

  // Creates a render process host based on a new site instance given the
  // |profile| and mark it as used. Returns a reference to it.
  content::RenderProcessHost* CreatedRendererProcessHost(Profile* profile) {
    site_instances_.emplace_back(content::SiteInstance::Create(profile));

    content::RenderProcessHost* rph = site_instances_.back()->GetProcess();
    EXPECT_TRUE(rph);
    rph->SetIsUsed();
    return rph;
  }

  void StopKeepingAliveOriginalProfile() {
    original_profile_keep_alive_.reset();
  }

  // Destroying profile is still not universally supported. We need to disable
  // some tests, because it isn't possible to start destroying the profile.
  bool IsScopedProfileKeepAliveSupported() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
    return false;
#else
    return base::FeatureList::IsEnabled(
        features::kDestroyProfileOnBrowserClose);
#endif
  }

 protected:
  const bool is_primary_otr_ = GetParam();

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  content::RenderViewHostTestEnabler rvh_test_enabler_;

  raw_ptr<TestingProfile> original_profile_;
  std::vector<raw_ptr<TestingProfile>> otr_profiles_;

  std::unique_ptr<ScopedProfileKeepAlive> original_profile_keep_alive_;
  std::vector<scoped_refptr<content::SiteInstance>> site_instances_;
};

TEST_P(ProfileDestroyerTest, DestroyOriginalProfileImmediately) {
  if (!IsScopedProfileKeepAliveSupported())
    return;
  CreateOriginalProfile();
  CreateOTRProfile();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(OtrProfile(0));

  StopKeepingAliveOriginalProfile();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(OtrProfile(0));

  // This doesn't really match real-world scenarios, because TestingProfile is
  // different from OffTheRecordProfileImpl. The real impl acquires a keepalive
  // on the parent profile, whereas OTR TestingProfile doesn't do that.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(original_profile());
  EXPECT_FALSE(OtrProfile(0));
}

TEST_P(ProfileDestroyerTest, DestroyOriginalProfileDeferedByRenderProcessHost) {
  if (!IsScopedProfileKeepAliveSupported())
    return;
  CreateOriginalProfile();
  CreateOTRProfile();
  content::RenderProcessHost* render_process_host =
      CreatedRendererProcessHost(original_profile());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(OtrProfile(0));

  // The original profile is not destroyed, because of the RenderProcessHost.
  StopKeepingAliveOriginalProfile();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(OtrProfile(0));

  // Releasing the RenderProcessHost triggers the deletion of the Profile. It
  // happens in a posted task.
  render_process_host->Cleanup();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(OtrProfile(0));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(original_profile());
  EXPECT_FALSE(OtrProfile(0));
}

TEST_P(ProfileDestroyerTest,
       DestroyOriginalProfileDeferedByOffTheRecordRenderProcessHost) {
  if (!IsScopedProfileKeepAliveSupported())
    return;
  CreateOriginalProfile();
  CreateOTRProfile();
  content::RenderProcessHost* render_process_host =
      CreatedRendererProcessHost(OtrProfile(0));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(OtrProfile(0));

  // The original profile is not destroyed, because of the RenderProcessHost.
  StopKeepingAliveOriginalProfile();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(OtrProfile(0));

  // Releasing the RenderProcessHost triggers the deletion of the Profile. It
  // happens in a posted task.
  render_process_host->Cleanup();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(OtrProfile(0));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(original_profile());
  EXPECT_FALSE(OtrProfile(0));
}

TEST_P(ProfileDestroyerTest,
       DetroyBothProfileDeferedByMultipleRenderProcessHost) {
  if (!IsScopedProfileKeepAliveSupported())
    return;
  CreateOriginalProfile();
  CreateOTRProfile();
  content::RenderProcessHost* rph_otr_profile =
      CreatedRendererProcessHost(OtrProfile(0));
  content::RenderProcessHost* rph_original_profile =
      CreatedRendererProcessHost(original_profile());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(OtrProfile(0));

  // No profile are destroyed, because of the RenderProcessHosts.
  StopKeepingAliveOriginalProfile();
  ProfileDestroyer::DestroyProfileWhenAppropriate(OtrProfile(0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(OtrProfile(0));

  // Release the first process. It causes the associated profile to be released.
  // This happens in a posted task.
  rph_otr_profile->Cleanup();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(OtrProfile(0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(original_profile());
  EXPECT_FALSE(OtrProfile(0));

  // Release the second process. It causes the associated profile to be
  // released. This happens in a posted task.
  rph_original_profile->Cleanup();
  EXPECT_TRUE(original_profile());
  EXPECT_FALSE(OtrProfile(0));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(original_profile());
  EXPECT_FALSE(OtrProfile(0));
}

// Expect immediate OTR profile destruction when no pending renderer
// process host exists.
TEST_P(ProfileDestroyerTest, ImmediateOTRProfileDestruction) {
  CreateOriginalProfile();
  CreateOTRProfile();
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(OtrProfile(0));

  // Ask for destruction of OTR profile, and expect immediate destruction.
  ProfileDestroyer::DestroyProfileWhenAppropriate(OtrProfile(0));
  EXPECT_FALSE(OtrProfile(0));
}

// Expect pending renderer process hosts delay OTR profile destruction.
TEST_P(ProfileDestroyerTest, DelayedOTRProfileDestruction) {
  CreateOriginalProfile();
  CreateOTRProfile();

  // Create two render process hosts.
  content::RenderProcessHost* render_process_host1 =
      CreatedRendererProcessHost(OtrProfile(0));
  content::RenderProcessHost* render_process_host2 =
      CreatedRendererProcessHost(OtrProfile(0));

  // Ask for destruction of OTR profile, but expect it to be delayed.
  ProfileDestroyer::DestroyProfileWhenAppropriate(OtrProfile(0));
  EXPECT_TRUE(OtrProfile(0));

  // Destroy the first pending render process host, and expect it not to destroy
  // the OTR profile.
  render_process_host1->Cleanup();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(OtrProfile(0));

  // Destroy the other renderer process, and expect destruction of OTR
  // profile.
  render_process_host2->Cleanup();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(OtrProfile(0));
}

// Regression test for:
// https://crbug.com/1337388#c11
TEST_P(ProfileDestroyerTest,
       DestructionRequestedTwiceWhileDelayedOriginalProfile) {
  if (!IsScopedProfileKeepAliveSupported())
    return;
  CreateOriginalProfile();

  content::RenderProcessHost* render_process_host =
      CreatedRendererProcessHost(original_profile());
  StopKeepingAliveOriginalProfile();

  EXPECT_TRUE(original_profile());
  ProfileDestroyer::DestroyProfileWhenAppropriate(original_profile());
  EXPECT_TRUE(original_profile());
  ProfileDestroyer::DestroyProfileWhenAppropriate(original_profile());
  EXPECT_TRUE(original_profile());

  render_process_host->Cleanup();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(original_profile());
}

// Regression test for:
// https://crbug.com/1337388#c11
TEST_P(ProfileDestroyerTest, DestructionRequestedTwiceWhileDelayedOTRProfile) {
  CreateOriginalProfile();
  CreateOTRProfile();

  content::RenderProcessHost* render_process_host =
      CreatedRendererProcessHost(OtrProfile(0));

  ProfileDestroyer::DestroyProfileWhenAppropriate(OtrProfile(0));
  EXPECT_TRUE(OtrProfile(0));
  ProfileDestroyer::DestroyProfileWhenAppropriate(OtrProfile(0));
  EXPECT_TRUE(OtrProfile(0));

  render_process_host->Cleanup();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(OtrProfile(0));
}

// Regression for: https://crbug.com/1357476
// When two OTR profile are associated with the same original profile,
// requesting the destruction of one, was incorrectly causing the
// ProfileDestroyer to wait for the destruction of the RenderProcessHost of the
// second OTR profile.
TEST_P(ProfileDestroyerTest, MultipleOTRPRofile) {
  CreateOriginalProfile();
  CreateOTRProfile();
  CreateOTRProfile();
  CreateOTRProfile();

  // Create a renderer process associated with every OTR profiles.
  content::RenderProcessHost* render_process_host[3] = {
      CreatedRendererProcessHost(OtrProfile(0)),
      CreatedRendererProcessHost(OtrProfile(1)),
      CreatedRendererProcessHost(OtrProfile(2)),
  };

  // Ask for the destruction of two of them. The destruction is delayed, because
  // they are kept alive by two RenderProcessHost depending on them.
  ProfileDestroyer::DestroyProfileWhenAppropriate(OtrProfile(0));
  ProfileDestroyer::DestroyProfileWhenAppropriate(OtrProfile(1));
  EXPECT_TRUE(original_profile());
  EXPECT_TRUE(OtrProfile(0));
  EXPECT_TRUE(OtrProfile(1));
  EXPECT_TRUE(OtrProfile(2));

  // Destroy the RenderProcessHost keeping alive the first OTR profile.
  render_process_host[0]->Cleanup();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(original_profile());
  EXPECT_FALSE(OtrProfile(0));
  EXPECT_TRUE(OtrProfile(1));
  EXPECT_TRUE(OtrProfile(2));

  // Destroy the RenderProcessHost keeping alive the second OTR profile.
  render_process_host[1]->Cleanup();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(original_profile());
  EXPECT_FALSE(OtrProfile(0));
  EXPECT_FALSE(OtrProfile(1));
  EXPECT_TRUE(OtrProfile(2));

  // Destroy the third RenderProcessHost.
  render_process_host[2]->Cleanup();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(original_profile());
  EXPECT_FALSE(OtrProfile(0));
  EXPECT_FALSE(OtrProfile(1));
  EXPECT_TRUE(OtrProfile(2));

  // Allow the deletion of the last OTR profile:
  ProfileDestroyer::DestroyProfileWhenAppropriate(OtrProfile(2));
  EXPECT_TRUE(original_profile());
  EXPECT_FALSE(OtrProfile(0));
  EXPECT_FALSE(OtrProfile(1));
  EXPECT_FALSE(OtrProfile(2));
}

INSTANTIATE_TEST_SUITE_P(AllOTRProfileTypes,
                         ProfileDestroyerTest,
                         /*is_primary_otr=*/testing::Bool());
