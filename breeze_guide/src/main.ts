import App from "./App.svelte";
import * as Sentry from "@sentry/browser";
import "@fontsource/unifont";
import { registerSW } from "virtual:pwa-register";
import { mount } from "svelte";

if (location.hostname !== "localhost")
  Sentry.init({
    dsn: process.env.SENTRY_DSN,
    integrations: [new Sentry.BrowserTracing()],
    tracesSampleRate: 0.2,
    ...(process.env.GITHUB_SHA && {
      release: `cdda-breeze@${process.env.GITHUB_SHA.slice(0, 8)}`,
    }),
  });

registerSW({});

if (location.hash) {
  history.replaceState(
    null,
    "",
    import.meta.env.BASE_URL + location.hash.slice(2) + location.search,
  );
}

mount(App, {
  target: document.body,
});