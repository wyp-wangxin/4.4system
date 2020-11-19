## -*- coding: utf-8 -*-
/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * ! Do not edit this file directly !
 *
 * Generated automatically from system/media/camera/docs/CameraCharacteristicsTest.mako.
 * This file contains only the auto-generated CameraCharacteristics CTS tests; it does
 * not contain any additional manual tests, which would be in a separate file.
 */

package android.hardware.camera2.cts;

import android.content.Context;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CameraMetadata.Key;
import android.test.AndroidTestCase;
import android.util.Log;

import java.util.List;

/**
 * Auto-generated CTS test for CameraCharacteristics fields.
 */
public class CameraCharacteristicsTest extends AndroidTestCase {
    private CameraManager mCameraManager;
    private static final String TAG = "CameraCharacteristicsTest";

    @Override
    public void setContext(Context context) {
        super.setContext(context);
        mCameraManager = (CameraManager)context.getSystemService(Context.CAMERA_SERVICE);
        assertNotNull("Can't connect to camera manager", mCameraManager);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
    }
    % for sec in find_all_sections(metadata):
      % for entry in find_unique_entries(sec):
        % if entry.kind == 'static' and entry.visibility == "public":

    public void testCameraCharacteristics${pascal_case(entry.name)}() throws Exception {
        String[] ids = mCameraManager.getCameraIdList();
        for (int i = 0; i < ids.length; i++) {
            CameraCharacteristics props = mCameraManager.getCameraCharacteristics(ids[i]);
            assertNotNull(String.format("Can't get camera characteristics from: ID %s", ids[i]),
                                        props);

          % if entry.applied_optional:
            Integer hwLevel = props.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL);
            assertNotNull("No hardware level reported! android.info.supportedHardwareLevel",
                    hwLevel);
            if (hwLevel == CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL_FULL)
          % endif
            {

                assertNotNull("Invalid property: ${entry.name}",
                        props.get(CameraCharacteristics.${jkey_identifier(entry.name)}));

                List<Key<?>> allKeys = props.getKeys();
                assertNotNull(String.format("Can't get camera characteristics keys from: ID %s",
                        ids[i], props));
                assertTrue("Key not in keys list: ${entry.name}", allKeys.contains(
                        CameraCharacteristics.${jkey_identifier(entry.name)}));

            }

        }
    }
        % endif
      % endfor
    % endfor
}

