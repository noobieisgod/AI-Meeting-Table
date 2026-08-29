# Synsemble

Multi-agent AI collaboration for structured planning, execution, review, and decision-making.

Synsemble is a multi-agent AI collaboration app that brings multiple AI roles together to plan, execute, review, revise, and produce a final result. It is not primarily an audio meeting recorder, transcription service, or meeting notetaker.

Synsemble was formerly developed under the name AI Meeting Table.

## Core workflow

1. Create a table.
2. Add AI seats one at a time.
3. Assign each seat a provider, model, and role.
4. Submit a task.
5. Let the agents research, plan, execute, and review.
6. Resolve targeted revisions through the decision workflow.
7. Produce the final artifact.

## Build and install

The Android app targets arm64 devices running Android 9 or newer. Build and deployment instructions are in [docs/ANDROID_DEPLOYMENT.md](docs/ANDROID_DEPLOYMENT.md). Production signing and Google Play publication remain manual.

## Why This Is Different From Generic Wrapper Software

This app is not just a thin user interface on top of provider APIs.

It is built around a meeting workflow:

- Multiple seats can be assigned to different providers and models in the same session.
- Each seat has a defined role, not just a label.
- The workflow moves through explicit phases such as research, planning, execution, quality control, and presentation.
- The software enforces authority rules at the application level instead of relying only on prompt wording.
- A Final Decision Maker can evaluate or rule on the meeting instead of every model acting as an equal free-form chatbot.
- The meeting keeps a shared transcript, event log, and artifact history instead of treating every prompt as an isolated chat turn.
- Attachments, artifacts, phase logic, and stop policies are part of the runtime model.

The app is designed to coordinate models as a structured team, not just expose several chat boxes behind one interface.

## Features

- Native Android app
- Multi-provider setup across OpenAI, Gemini, and Anthropic
- Seat-based model configuration
- Role-based collaboration
- Phase-driven workflow
- Final Decision Maker or judge role
- Shared transcript and event log
- Shared evolving artifact output
- Attachment support
- Configurable hard stops for tokens, rounds, loops, and time
- Pause, resume, continue, and follow-up meeting flow
- Local persistence of meetings and UI state

## Why the App Is Not Stealing Your API Keys

If you are worried about API key safety, this is how the app handles them:

- The app is open source, so the code can be inspected.
- On Android, API keys are protected by the Android Keystore-backed credential bridge rather than being written plainly into the project folder.
- The app loads those keys only when it needs to make a provider request.
- The keys are then sent directly to the selected provider API as normal authentication headers.
- The app does not require you to log into a central Synsemble account.
- The app does not route your API calls through a custom Synsemble backend service.

That means the app is not designed around collecting and proxying your keys through someone else's server. It is a local Android client that uses your own provider credentials to talk to the provider you selected.

You should still use normal caution:

- Only download releases from the official repository.
- If you want the highest level of confidence, review the source code and build it yourself.
- Rotate your API keys if you believe your machine has been compromised.

## License

This project is licensed under the GNU Affero General Public License v3.0.

Full license text:
https://github.com/noobieisgod/Synsemble/blob/main/LICENSE
