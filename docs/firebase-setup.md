# Firebase configuration checklist

1. Use the correct Firebase project and Realtime Database URL from Firebase Console.
2. In Authentication, enable Email/Password and use a dedicated device/dashboard user. Change the password exposed during troubleshooting.
3. Check Realtime Database rules. Do not deploy public read/write rules. Restrict access to intended user/device IDs.
4. Keep service-account JSON outside Git. It is used only by the optional Admin SDK Twilio worker, not by the dashboard or firmware login.
5. Revoke/delete the previously exposed Admin SDK key in Google Cloud IAM before generating a replacement.
6. Restrict the Firebase Web API key to the required APIs and intended app usage.
