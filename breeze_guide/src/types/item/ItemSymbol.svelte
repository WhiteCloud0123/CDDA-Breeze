<script lang="ts">
import { colorForName } from "../../colors";
export let item: {
  id: string;
  looks_like?: string;
  color?: string | string[];
  bgcolor?: string | [string] | [string, string, string, string];
  symbol?: string | string[];
  type: string;
  variants?: any;
};

const sym =
  item.type === "vehicle_part" && item.variants
    ? item.variants[0].symbols[0]
    : ([item.symbol].flat()[0] ?? " ");
const symbol = /^LINE_/.test(sym) ? "|" : sym;
const color = item.color
  ? typeof item.color === "string"
    ? item.color
    : item.color[0]
  : item.bgcolor
    ? colorFromBgcolor(item.bgcolor)
    : "white";

function colorFromBgcolor(
  color: string | [string] | [string, string, string, string],
): string {
  return typeof color === "string" ? `i_${color}` : colorFromBgcolor(color[0]);
}
</script>

<span style="font-family: monospace;" class="c_{color}">{symbol}</span>