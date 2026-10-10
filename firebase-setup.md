# Firebase configuration checklist

1. Use the correct Firebase project and Realtime Database URL from Firebase Console.
2. In Authentication, enable the Email/Password provider and create a dedicated device/dashboard user. Use a new password after rotating the credentials exposed during troubleshooting.
3. Check Realtime Database rules. Do not deploy public read/write rules. Restrict paths to only intended user/device IDs; the sample firmware uses the same configured Firebase Authentication identity for REST writes.
4. Keep a service-account JSON outside Git. It is only used for the optional Admin SDK Twilio worker, not for the dashboard or firmware login.
5. If you regenerate an Admin SDK key, revoke/delete the previously exposed key in Google Cloud IAM first.
6. API keys are not equivalent to service-account private keys, but restrict the Firebase Web API key to required APIs and appropriate web/app usage.
