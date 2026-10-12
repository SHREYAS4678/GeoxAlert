# Firebase configuration checklist

1. Confirm the exact Realtime Database URL and region in Firebase Console.
2. Enable Email/Password in Firebase Authentication and use a dedicated dashboard/device account. Rotate credentials that were shared during earlier troubleshooting.
3. Firmware writes live telemetry to `/nodes/<node_id>/live` and history to `/nodes/<node_id>/history`.
4. Streamlit stores recipients under `/nodes/<node_id>/contacts` and queues multi-recipient SMS jobs under `/nodes/<node_id>/alerts/queue`. The ESP32/SIM800L reads jobs and updates per-recipient status.
5. Review `database.rules.json` before deploying. It allows authenticated access to node data and writes to the required live/history/contact/queue paths; it is not a full per-user authorization design.
6. After verifying that replacing your current database rules is intended, deploy with:
   ```powershell
   firebase login
   firebase use geoalert-ai-ee1fd
   firebase deploy --only database --project geoalert-ai-ee1fd
   ```
7. The redesigned firmware/dashboard use Firebase Authentication REST tokens and do not require an Admin SDK service-account file. Revoke/delete the previously exposed service-account key in Google Cloud IAM.
8. Restrict the Firebase Web API key to the APIs and usage needed by the project. Never commit local `secrets.h`, Streamlit `secrets.toml`, service-account JSON or backups.
