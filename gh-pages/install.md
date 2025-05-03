---
title: Web installer
---
<script type="module" src="https://unpkg.com/esp-web-tools@10.1/dist/web/install-button.js?module"></script>
<style>
.install-button, select {
  background-color: #2f80ed;
  border-radius: 10px;
  border-style: none;
  color: #fff;
  font-family: Inter,-apple-system,system-ui,"Segoe UI",Helvetica,Arial,sans-serif;
  font-size: 15px;
  font-weight: 500;
  height: 50px;
  line-height: 1.5;
  outline: none;
  padding: 14px 30px;
  margin-top: 3px;
  transition: all .3s;
  user-select: none;
  cursor: pointer;
  overflow: hidden;
  text-wrap: nowrap;
}

.install-button:hover, select:hover {
  background-color: #1366d6;
  box-shadow: rgba(0, 0, 0, .05) 0 5px 30px, rgba(0, 0, 0, .05) 0 1px 4px;
  opacity: 1;
  transform: translateY(0);
  transition-duration: .35s;
}

.install-button:hover:after {
  opacity: .5;
}


select {
  padding-right: 45px;
  outline: 0;

  margin: 0;      
  -webkit-box-sizing: border-box;
  -moz-box-sizing: border-box;
  box-sizing: border-box;
  -webkit-appearance: none;
  -moz-appearance: none;

  background-image:
    linear-gradient(45deg, transparent 50%, #fff 50%),
    linear-gradient(135deg, #fff 50%, transparent 50%);
  background-position:
    calc(100% - 18px) calc(50% + 2px),
    calc(100% - 11px) calc(50% + 2px);
  background-size:
    7px 7px,
    7px 7px;
  background-repeat: no-repeat;
}

</style>

1. Plug your ESP into a USB port. 
2. Select the firmware version you want and hit `install`.
3. Select the correct com port, and follow the instructions.

<p>
<select id="manifests">
{% for manifest in site.data.manifests %}
  <option value="{{ manifest.path }}">{{ manifest.name }}</option>
{% endfor %}
</select>
</p>

<esp-web-install-button id="install-button"
  manifest="publish/ATEM_tally_light_manifest.json"
>
  <button class="install-button" slot="activate">Install</button>
</esp-web-install-button>
<br>
Powered by [ESP Web Tools](https://esphome.github.io/esp-web-tools/)

<script>
  var manifestSelect = document.getElementById("manifests")
  var installButton = document.getElementById("install-button")
  installButton.setAttribute("manifest", manifestSelect.value)

  manifestSelect.onchange = (e) => installButton.setAttribute("manifest", e.target.value)
</script>

<br>
<br>
Thank you for using my work!<br>
If you like it please consider supporting. 

<a href="https://www.buymeacoffee.com/aronhetlam" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" style="height: 60px !important;width: 217px !important;" ></a>
