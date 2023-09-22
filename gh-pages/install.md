---
title: Web installer
---
<script type="module" src="https://unpkg.com/esp-web-tools@9/dist/web/install-button.js?module"></script>
<style>
.install-button {
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
  transform: translate3d(0, 0, 0);
  transition: all .3s;
  user-select: none;
  cursor: pointer;
  overflow: hidden;
  text-wrap: nowrap;
}

.install-button:hover {
  background-color: #1366d6;
  box-shadow: rgba(0, 0, 0, .05) 0 5px 30px, rgba(0, 0, 0, .05) 0 1px 4px;
  opacity: 1;
  transform: translateY(0);
  transition-duration: .35s;
}

.install-button:hover:after {
  opacity: .5;
}

@media (min-width: 768px) {
  .install-button {
    padding: 14px 22px;
    width: 176px;
  }
}
</style>

1. Plug in your ESP to a USB port. 
2. Hit the install button for the firmware you wish to install.
3. Select the correct com port, and follow the instructions.

<esp-web-install-button
  manifest="publish/ATEM_tally_light_manifest.json"
>
  <button class="install-button" slot="activate">Install Tally light</button>
</esp-web-install-button>
<esp-web-install-button
  manifest="publish/ATEM_tally_test_server_manifest.json"
>
  <button class="install-button" slot="activate">Install Test server</button>
  <span slot="unsupported"/>
  <span slot="not-allowed"/>
</esp-web-install-button>
<br>
Powered by [ESP Web Tools](https://esphome.github.io/esp-web-tools/)

<br>
<br>
Thank you for using my work!<br>
If you like it please consider supporting. 

<a href="https://www.buymeacoffee.com/aronhetlam" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" style="height: 60px !important;width: 217px !important;" ></a>
