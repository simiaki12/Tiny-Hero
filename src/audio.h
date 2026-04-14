#pragma once

/* Start the world overworld theme (loops indefinitely). */
void audioPlayWorld(void);

/* Stop any currently playing music. */
void audioStop(void);

/* Volume: 0 (silent) to 10 (full). Default 8. */
void audioSetVolume(int vol);
int  audioGetVolume(void);

/* Release resources on shutdown. */
void audioCleanup(void);
