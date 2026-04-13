#pragma once

/* Start the world overworld theme (loops indefinitely). */
void audioPlayWorld(void);

/* Stop any currently playing music. */
void audioStop(void);

/* Release resources on shutdown. */
void audioCleanup(void);
