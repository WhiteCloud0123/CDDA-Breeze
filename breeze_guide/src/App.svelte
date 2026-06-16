<script lang="ts">
import Thing from "./Thing.svelte";
import { CddaData, data, loadProgress, localeVersion, mapType, singularName } from "./data";
import SearchResults from "./SearchResults.svelte";
import Catalog from "./Catalog.svelte";
import dontPanic from "./assets/dont_panic.png";
import InterpolatedTranslation from "./InterpolatedTranslation.svelte";
import { t } from "@transifex/native";
import type { SupportedTypeMapped, SupportedTypesWithMapped } from "./types";
import throttle from "lodash/throttle";
import debounce from "lodash/debounce";
import { onDestroy } from "svelte";

let item: { type: string; id: string } | null = null;
let search: string = "";
let renderedSearch = search;
const updateRenderedSearch = debounce((value: string) => {
  renderedSearch = value;
}, 150);

function renderSearchNow() {
  updateRenderedSearch.cancel();
  renderedSearch = search;
}

$: if (search !== renderedSearch) {
  if (search) updateRenderedSearch(search);
  else renderSearchNow();
}

onDestroy(updateRenderedSearch.cancel);

data.load();

function decodeQueryParam(p: string) {
  return decodeURIComponent(p.replace(/\+/g, " "));
}

function load() {
  const path = location.pathname.slice(import.meta.env.BASE_URL.length - 1);
  let m: RegExpExecArray | null;
  if ((m = /^\/([^\/]+)(?:\/(.+))?$/.exec(path))) {
    const [, type, id] = m;
    if (type === "search") {
      item = null;
      search = decodeQueryParam(id ?? "");
      renderSearchNow();
    } else {
      item = { type, id: id ? decodeURIComponent(id) : "" };
    }

    window.scrollTo(0, 0);
  } else {
    item = null;
    search = "";
    renderSearchNow();
  }
}

$: if (item && item.id && $data && $data.byIdMaybe(item.type as any, item.id)) {
  const it = $data.byId(item.type as any, item.id);
  document.title = `${singularName(
    it,
  )} - Breeze Guide`;
} else if (item && !item.id && item.type) {
  document.title = `${item.type} - Breeze Guide`;
} else {
  document.title = "Breeze Guide";
}

load();

// Throttle replaceState to avoid browser warnings.
// |throttle| isn't defined when running tests for some reason.
const replaceState = throttle
  ? throttle(history.replaceState.bind(history), 100, {
      trailing: true,
    })
  : history.replaceState.bind(history);

const clearItem = () => {
  if (item)
    history.pushState(
      null,
      "",
      import.meta.env.BASE_URL +
        (search ? "search/" + encodeURIComponent(search) : "") +
        location.search,
    );
  else
    replaceState(
      null,
      "",
      import.meta.env.BASE_URL +
        (search ? "search/" + encodeURIComponent(search) : "") +
        location.search,
    );
  item = null;
};

function maybeNavigate(event: MouseEvent) {
  const target = event.target as HTMLElement | null;
  const anchor = target?.closest("a") as HTMLAnchorElement | null;
  if (anchor && anchor.href) {
    const { origin, pathname } = new URL(anchor.href);
    if (
      origin === location.origin &&
      pathname.startsWith(import.meta.env.BASE_URL)
    ) {
      event.preventDefault();
      history.pushState(null, "", pathname + location.search);
      load();
    }
  }
}

window.addEventListener("popstate", () => {
  load();
});

let deferredPrompt: any;
window.addEventListener("beforeinstallprompt", (e) => {
  deferredPrompt = e;
});

function maybeFocusSearch(e: KeyboardEvent) {
  if (e.key === "/" && document.activeElement?.id !== "search") {
    document.getElementById("search")?.focus();
    e.preventDefault();
  }
}

const randomableItemTypes = new Set<keyof SupportedTypesWithMapped>([
  "item",
  "monster",
  "furniture",
  "terrain",
  "vehicle_part",
  "tool_quality",
  "mutation",
  "martial_art",
  "json_flag",
  "achievement",
  "conduct",
  "proficiency",
]);
async function getRandomPage() {
  const d = await new Promise<CddaData>((resolve) => {
    const unsubscribe = data.subscribe((v) => {
      if (v) {
        resolve(v);
        setTimeout(() => unsubscribe());
      }
    });
  });
  const items = d
    .all()
    .filter(
      (x) => "id" in x && randomableItemTypes.has(mapType(x.type)),
    ) as (SupportedTypeMapped & { id: string })[];
  return items[(Math.random() * items.length) | 0];
}

let randomPage: string | null = null;
function newRandomPage() {
  getRandomPage().then((r) => {
    randomPage = `${import.meta.env.BASE_URL}${mapType(r.type)}/${r.id}${
      location.search
    }`;
  });
}
newRandomPage();
</script>

<svelte:window on:click={maybeNavigate} on:keydown={maybeFocusSearch} />

<header>
  <nav>
    <div class="title">
      <!-- svelte-ignore a11y-invalid-attribute -->
      <strong>
        <a
          href={import.meta.env.BASE_URL + location.search}
          on:click={() => (search = "")}
          ><span class="wide">Breeze Guide</span><span
            class="narrow">BG</span
          ></a>
      </strong>
    </div>
    <div class="search">
      <input
        style="margin: 0; width: 100%"
        placeholder={t("搜索...")}
        type="search"
        bind:value={search}
        on:input={clearItem}
        id="search" />
    </div>
  </nav>
</header>
<main>
  {#key $localeVersion}
  {#if item}
    {#if $data}
      {#key item}
        {#if item.id}
          <Thing {item} data={$data} />
        {:else}
          <Catalog type={item.type} data={$data} />
        {/if}
      {/key}
    {:else}
      <span style="color: var(--cata-color-gray)">
        <em>{t("加载中...")}</em>
        {#if $loadProgress}
          ({($loadProgress[0] / 1024 / 1024).toFixed(1)}/{(
            $loadProgress[1] /
            1024 /
            1024
          ).toFixed(1)} MB)
        {/if}
      </span>
    {/if}
  {:else if search}
    {#if $data}
      {#key renderedSearch}
        <SearchResults data={$data} search={renderedSearch} />
      {/key}
    {:else}
      <span style="color: var(--cata-color-gray)">
        <em>{t("加载中...")}</em>
        {#if $loadProgress}
          ({($loadProgress[0] / 1024 / 1024).toFixed(1)}/{(
            $loadProgress[1] /
            1024 /
            1024
          ).toFixed(1)} MB)
        {/if}
      </span>
    {/if}
  {:else}
    <img
      src={dontPanic}
      height="200"
      width="343"
      style="float:right"
      alt="大大的友善字体写着'不要慌'" />
    <p>
      <InterpolatedTranslation
        str={t(
          `{hhg} 是 CDDA-Breeze 的指南。你可以搜索游戏中的物品（如{link_flashlight}）、家具（如{link_table}）或怪物（如{link_zombie}），并查看它们的详细信息。数据来源于游戏本身的 JSON 文件。`,
          {
            hhg: "{hhg}",
            link_flashlight: "{link_flashlight}",
            link_table: "{link_table}",
            link_zombie: "{link_zombie}",
          },
        )}
        slot0="hhg"
        slot1="link_flashlight"
        slot2="link_table"
        slot3="link_zombie">
        <strong slot="0">Breeze Guide</strong>
        <a
          slot="1"
          href="{import.meta.env.BASE_URL}item/flashlight{location.search}"
          >{t("flashlight", { _comment: "Item name" })}</a>
        <a
          slot="2"
          href="{import.meta.env.BASE_URL}furniture/f_table{location.search}"
          >{t("table", { _comment: "Furniture" })}</a>
        <a
          slot="3"
          href="{import.meta.env.BASE_URL}monster/mon_zombie{location.search}"
          >{t("zombie", { _comment: "Monster name" })}</a>
      </InterpolatedTranslation>
    </p>
    <p>
      {t(`数据存储在本地，支持离线使用。`)}
      {#if deferredPrompt}
        <InterpolatedTranslation
          str={t(
            `也可以将其{installable_button}，脱离浏览器像普通应用一样使用。`,
            { installable_button: "{installable_button}" },
          )}
          slot0="installable_button">
          <button
            slot="0"
            class="disclosure"
            on:click={(e) => {
              e.preventDefault();
              deferredPrompt.prompt();
            }}
            >{t("安装")}</button>
        </InterpolatedTranslation>
      {/if}
    </p>

    <h2>{t("目录")}</h2>
    <ul>
      <li><a href="/item{location.search}">{t("物品")}</a></li>
      <li><a href="/monster{location.search}">{t("怪物")}</a></li>
      <li><a href="/furniture{location.search}">{t("家具")}</a></li>
      <li><a href="/terrain{location.search}">{t("地形")}</a></li>
      <li><a href="/vehicle_part{location.search}">{t("车辆部件")}</a></li>
      <li><a href="/tool_quality{location.search}">{t("工具质量")}</a></li>
      <li><a href="/mutation{location.search}">{t("变异")}</a></li>
      <li><a href="/martial_art{location.search}">{t("武术")}</a></li>
      <li><a href="/json_flag{location.search}">{t("标志")}</a></li>
      <li>
        <a href="/achievement{location.search}">{t("成就")}</a> /
        <a href="/conduct{location.search}">{t("行为")}</a>
      </li>
      <li><a href="/proficiency{location.search}">{t("专精")}</a></li>
    </ul>

    <InterpolatedTranslation
      str={t(`或访问{link_random_page}。`, {
        link_random_page: "{link_random_page}",
      })}
      slot0="link_random_page">
      <a slot="0" href={randomPage} on:click={() => setTimeout(newRandomPage)}
        >{t("随机页面")}</a>
    </InterpolatedTranslation>
  {/if}

  <p class="data-options">
    {t("版本:")}
    {#if $data}
      <strong>{$data.build_number}</strong>
    {:else}
      <em style="color: var(--cata-color-gray)">({t("Loading...")})</em>
    {/if}
  </p>
  {/key}
</main>

<style>
main {
  text-align: left;
  padding: 1em;
  max-width: 980px;
  margin: 0 auto;
  margin-top: 4rem;
}
header {
  position: fixed;
  top: 0;
  left: 0;
  z-index: 100;
  box-shadow: 0px 0px 8px rgba(0, 0, 0, 0.5);
  width: 100%;
  height: 4rem;
  background: rgba(33, 33, 33, 0.98);
  padding: 0 calc(1em + 8px);
  box-sizing: border-box;
}

nav {
  max-width: 980px;
  margin: 0 auto;
  display: flex;
  align-items: center;
  justify-content: space-between;
  height: 100%;
}

nav > .search {
  flex: 1;
  max-width: calc(0.5 * 980px);
}

nav > .title .narrow {
  display: none;
}

nav > .title {
  margin-right: 1em;
}

@media (max-width: 600px) {
  nav > .title .wide {
    display: none;
  }
  nav > .title .narrow {
    display: inline;
  }

  nav > .search {
    flex: 1;
  }
}

.data-options select {
  max-width: 100%;
}
</style>
