# AI Meeting Table

AI Meeting Table is a native Windows app where multiple AI models work together in a structured, phase-driven workflow with defined roles, software-level authority rules, and a shared artifact used to solve a user task.

## How to Install

1. Go to [https://github.com/noobieisgod/AI-Meeting-Table/releases](https://github.com/noobieisgod/AI-Meeting-Table/releases).
2. Download the latest ZIP release.
3. Unzip the ZIP file.
4. Open the unzipped folder.
5. Run `AIMeetingTable.exe`.

## How to Set Up

1. Open **Settings**.
2. Under **Provider Credentials**, enter the API keys you want to use:
   - OpenAI API Key
   - Gemini API Key
   - Anthropic API Key
3. Optional: click **Refresh Models** after adding your keys so the app can fetch the latest available models from your configured providers.
4. Create a new meeting table or edit an existing one.
5. Configure the seats in the meeting table:
   - Turn on **Seat is occupied** for each seat you want to use.
   - Give each seat a clear **Display Name**.
   - Choose a **Provider**.
   - Choose a **Model**.
   - Choose an **Effort** level if that model supports it.
   - Choose a **Role**.
6. Make sure you have at least:
   - one occupied non-final participant seat
   - one **Final Decision Maker** seat if you want a judge/decision role
7. Type your task or instruction into the message box in the transcript pane.
8. Click **Send**.
9. Click **Run Session**.

### Optional Setup Tips

- Use **Lead Planner**, **Lead Executioner**, and **Lead Quality Control** if you want a more structured workflow.
- Use **Add Attachment** if you want the meeting to work with files.
- Use **Hard Stops** in **Settings** if you want token, cost, round, or time limits.

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

- Native Windows desktop app
- Multi-provider setup across OpenAI, Gemini, and Anthropic
- Seat-based model configuration
- Role-based collaboration
- Phase-driven workflow
- Final Decision Maker / judge role
- Shared transcript and event log
- Shared evolving artifact output
- Attachment support
- Configurable hard stops for tokens, cost, rounds, and time
- Pause, resume, continue, and follow-up meeting flow
- Local persistence of meetings and UI state

## Why the App Is Not Stealing Your API Keys

If you are worried about API key safety, this is how the app handles them:

- The app is open source, so the code can be inspected.
- On Windows, API keys are stored through the Windows Credential Manager rather than being written plainly into the project folder.
- The app loads those keys only when it needs to make a provider request.
- The keys are then sent directly to the selected provider API as normal authentication headers.
- The app does not require you to log into a central AI Meeting Table account.
- The app does not route your API calls through a custom AI Meeting Table backend service.

That means the app is not designed around collecting and proxying your keys through someone else’s server. It is a local desktop client that uses your own provider credentials to talk to the provider you selected.

You should still use normal caution:

- Only download releases from the official repository.
- If you want the highest level of confidence, review the source code and build it yourself.
- Rotate your API keys if you believe your machine has been compromised.

## License

This project is licensed under the GNU Affero General Public License v3.0.

See [LICENSE.txt](LICENSE.txt) for the full license text.
