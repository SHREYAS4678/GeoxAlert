# Firebase configuration checklist

1. Use the correct Firebase project and Realtime Database URL from Firebase Console.
2. In Authentication, enable Email/Password and use a dedicated device/dashboard user. Change the Firebase Authentication password exposed during troubleshooting.
3. Check Realtime Database rules. Do not deploy public read/write rules. Restrict access to intended user/device IDs.
4. The current ESP32 firmware and Streamlit dashboard use Firebase Authentication credentials and the Realtime Database REST API. They do not require an Admin SDK service-account JSON.
5. Revoke/delete the previously exposed Admin SDK service-account key in Google Cloud IAM, even though this version no longer uses it.
6. Restrict the Firebase Web API key to the required APIs and intended app usage.
7. Verify the firmware's node ID matches the dashboard selection; expected paths are `/nodes/<node_id>/live` and `/nodes/<node_id>/history`.
