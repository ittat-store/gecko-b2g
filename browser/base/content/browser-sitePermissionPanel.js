/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

/**
 * Utility object to handle manipulations of the identity permission indicators
 * in the UI.
 */
var gPermissionPanel = {
  _popupInitialized: false,
  _initializePopup() {
    if (!this._popupInitialized) {
      let wrapper = document.getElementById("template-permission-popup");
      wrapper.replaceWith(wrapper.content);
      this._popupInitialized = true;
    }
  },

  hidePopup() {
    if (this._popupInitialized) {
      PanelMultiView.hidePopup(this._permissionPopup);
    }
  },

  // smart getters
  get _identityPermissionBox() {
    delete this._identityPermissionBox;
    return (this._identityPermissionBox = document.getElementById(
      "identity-permission-box"
    ));
  },
  get _permissionGrantedIcon() {
    delete this._permissionGrantedIcon;
    return (this._permissionGrantedIcon = document.getElementById(
      "permissions-granted-icon"
    ));
  },
  get _permissionPopup() {
    if (!this._popupInitialized) {
      return null;
    }
    delete this._permissionPopup;
    return (this._permissionPopup = document.getElementById(
      "permission-popup"
    ));
  },
  get _permissionPopupMainView() {
    delete this._permissionPopupPopupMainView;
    return (this._permissionPopupPopupMainView = document.getElementById(
      "permission-popup-mainView"
    ));
  },
  get _permissionPopupMainViewHeaderLabel() {
    delete this._permissionPopupMainViewHeaderLabel;
    return (this._permissionPopupMainViewHeaderLabel = document.getElementById(
      "permission-popup-mainView-panel-header-span"
    ));
  },
  get _permissionList() {
    delete this._permissionList;
    return (this._permissionList = document.getElementById(
      "permission-popup-permission-list"
    ));
  },
  get _defaultPermissionAnchor() {
    delete this._defaultPermissionAnchor;
    return (this._defaultPermissionAnchor = document.getElementById(
      "permission-popup-permission-list-default-anchor"
    ));
  },
  get _permissionReloadHint() {
    delete this._permissionReloadHint;
    return (this._permissionReloadHint = document.getElementById(
      "permission-popup-permission-reload-hint"
    ));
  },
  get _permissionAnchors() {
    delete this._permissionAnchors;
    let permissionAnchors = {};
    for (let anchor of document.getElementById("blocked-permissions-container")
      .children) {
      permissionAnchors[anchor.getAttribute("data-permission-id")] = anchor;
    }
    return (this._permissionAnchors = permissionAnchors);
  },

  get _geoSharingIcon() {
    delete this._geoSharingIcon;
    return (this._geoSharingIcon = document.getElementById("geo-sharing-icon"));
  },

  get _xrSharingIcon() {
    delete this._xrSharingIcon;
    return (this._xrSharingIcon = document.getElementById("xr-sharing-icon"));
  },

  get _webRTCSharingIcon() {
    delete this._webRTCSharingIcon;
    return (this._webRTCSharingIcon = document.getElementById(
      "webrtc-sharing-icon"
    ));
  },

  /**
   * Refresh the contents of the permission popup. This includes the headline
   * and the list of permissions.
   */
  _refreshPermissionPopup() {
    let host = gIdentityHandler.getHostForDisplay();

    // Update header label
    this._permissionPopupMainViewHeaderLabel.textContent = gNavigatorBundle.getFormattedString(
      "permissions.header",
      [host]
    );

    // Refresh the permission list
    this.updateSitePermissions();
  },

  /**
   * Called by gIdentityHandler to hide permission icons for invalid proxy
   * state.
   */
  hidePermissionIcons() {
    this._identityPermissionBox.removeAttribute("hasGrantedPermissions");
    this._identityPermissionBox.removeAttribute("hasPermissionIcon");
  },

  /**
   * Updates the permissions icons in the identity block.
   * We show icons for blocked permissions / popups.
   */
  refreshPermissionIcons() {
    let permissionAnchors = this._permissionAnchors;

    // hide all permission icons
    for (let icon of Object.values(permissionAnchors)) {
      icon.removeAttribute("showing");
    }

    // keeps track if we should show an indicator that there are active permissions
    let hasGrantedPermissions = false;
    let hasPermissionIcon = false;

    // show permission icons
    let permissions = SitePermissions.getAllForBrowser(
      gBrowser.selectedBrowser
    );
    for (let permission of permissions) {
      if (
        permission.state == SitePermissions.BLOCK ||
        permission.state == SitePermissions.AUTOPLAY_BLOCKED_ALL
      ) {
        let icon = permissionAnchors[permission.id];
        if (icon) {
          icon.setAttribute("showing", "true");
          hasPermissionIcon = true;
        }
      } else if (permission.state != SitePermissions.UNKNOWN) {
        hasGrantedPermissions = true;
      }
    }

    // Show blocked popup icon in the identity-box if popups are blocked
    // irrespective of popup permission capability value.
    if (gBrowser.selectedBrowser.popupBlocker.getBlockedPopupCount()) {
      let icon = permissionAnchors.popup;
      icon.setAttribute("showing", "true");
      hasPermissionIcon = true;
    }

    this._identityPermissionBox.toggleAttribute(
      "hasGrantedPermissions",
      hasGrantedPermissions
    );
    this._identityPermissionBox.toggleAttribute(
      "hasPermissionIcon",
      hasPermissionIcon
    );
  },

  /**
   * Shows the permission popup.
   * @param {Event} event - Event which caused the popup to show.
   */
  _openPopup(event) {
    // Make the popup available.
    this._initializePopup();

    // Remove the reload hint that we show after a user has cleared a permission.
    this._permissionReloadHint.hidden = true;

    // Update the popup strings
    this._refreshPermissionPopup();

    // Add the "open" attribute to the button in the identity box for styling
    this._identityPermissionBox.setAttribute("open", "true");

    // Check the panel state of other panels. Hide them if needed.
    let openPanels = Array.from(document.querySelectorAll("panel[openpanel]"));
    for (let panel of openPanels) {
      PanelMultiView.hidePopup(panel);
    }

    // Now open the popup, anchored off the primary chrome element
    PanelMultiView.openPopup(
      this._permissionPopup,
      this._identityPermissionBox,
      {
        position: "bottomcenter topleft",
        triggerEvent: event,
      }
    ).catch(Cu.reportError);
  },

  /**
   * Update identity permission indicators based on sharing state of the
   * selected tab. This should be called externally whenever the sharing state
   * of the selected tab changes.
   */
  updateSharingIndicator() {
    let tab = gBrowser.selectedTab;
    this._sharingState = tab._sharingState;

    this._webRTCSharingIcon.removeAttribute("paused");
    this._webRTCSharingIcon.removeAttribute("sharing");
    this._geoSharingIcon.removeAttribute("sharing");
    this._xrSharingIcon.removeAttribute("sharing");

    let hasSharingIcon = false;

    if (this._sharingState) {
      if (this._sharingState.webRTC?.sharing) {
        this._webRTCSharingIcon.setAttribute(
          "sharing",
          this._sharingState.webRTC.sharing
        );
        hasSharingIcon = true;

        if (this._sharingState.webRTC?.paused) {
          this._webRTCSharingIcon.setAttribute("paused", "true");
        }
      }
      if (this._sharingState.geo) {
        this._geoSharingIcon.setAttribute("sharing", this._sharingState.geo);
        hasSharingIcon = true;
      }
      if (this._sharingState.xr) {
        this._xrSharingIcon.setAttribute("sharing", this._sharingState.xr);
        hasSharingIcon = true;
      }
    }

    this._identityPermissionBox.toggleAttribute(
      "hasSharingIcon",
      hasSharingIcon
    );

    if (this._popupInitialized && this._permissionPopup.state != "closed") {
      this.updateSitePermissions();
    }
  },

  /**
   * Click handler for the permission-box element in primary chrome.
   */
  handleIdentityButtonEvent(event) {
    event.stopPropagation();

    if (
      (event.type == "click" && event.button != 0) ||
      (event.type == "keypress" &&
        event.charCode != KeyEvent.DOM_VK_SPACE &&
        event.keyCode != KeyEvent.DOM_VK_RETURN)
    ) {
      return; // Left click, space or enter only
    }

    // Don't allow left click, space or enter if the location has been modified,
    // so long as we're not sharing any devices.
    // If we are sharing a device, the identity block is prevented by CSS from
    // being focused (and therefore, interacted with) by the user. However, we
    // want to allow opening the identity popup from the device control menu,
    // which calls click() on the identity button, so we don't return early.
    if (
      !this._sharingState &&
      gURLBar.getAttribute("pageproxystate") != "valid"
    ) {
      return;
    }

    // If we are in DOM fullscreen, exit it before showing the permission popup
    // (see bug 1557041)
    if (document.fullscreen) {
      // Open the identity popup after DOM fullscreen exit
      // We need to wait for the exit event and after that wait for the fullscreen exit transition to complete
      // If we call _openPopup before the fullscreen transition ends it can get cancelled
      // Only waiting for painted is not sufficient because we could still be in the fullscreen enter transition.
      this._exitedEventReceived = false;
      this._event = event;
      Services.obs.addObserver(this, "fullscreen-painted");
      window.addEventListener(
        "MozDOMFullscreen:Exited",
        () => {
          this._exitedEventReceived = true;
        },
        { once: true }
      );
      document.exitFullscreen();
      return;
    }

    this._openPopup(event);
  },

  onPopupShown(event) {
    if (event.target == this._permissionPopup) {
      window.addEventListener("focus", this, true);
    }
  },

  onPopupHidden(event) {
    if (event.target == this._permissionPopup) {
      window.removeEventListener("focus", this, true);
      this._identityPermissionBox.removeAttribute("open");
    }
  },

  handleEvent(event) {
    let elem = document.activeElement;
    let position = elem.compareDocumentPosition(this._permissionPopup);

    if (
      !(
        position &
        (Node.DOCUMENT_POSITION_CONTAINS | Node.DOCUMENT_POSITION_CONTAINED_BY)
      ) &&
      !this._permissionPopup.hasAttribute("noautohide")
    ) {
      // Hide the panel when focusing an element that is
      // neither an ancestor nor descendant unless the panel has
      // @noautohide (e.g. for a tour).
      PanelMultiView.hidePopup(this._permissionPopup);
    }
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "fullscreen-painted": {
        if (subject != window || !this._exitedEventReceived) {
          return;
        }
        Services.obs.removeObserver(this, "fullscreen-painted");
        this._openPopup(this._event);
        delete this._event;
        break;
      }
    }
  },

  onLocationChange() {
    if (this._popupInitialized && this._permissionPopup.state != "closed") {
      this._permissionReloadHint.hidden = true;
    }
  },

  /**
   * Updates the permission list in the permissions popup.
   */
  updateSitePermissions() {
    let permissionItemSelector = [
      ".permission-popup-permission-item, .permission-popup-permission-item-container",
    ];
    this._permissionList
      .querySelectorAll(permissionItemSelector)
      .forEach(e => e.remove());

    let permissions = SitePermissions.getAllPermissionDetailsForBrowser(
      gBrowser.selectedBrowser
    );

    this._sharingState = gBrowser.selectedTab._sharingState;

    if (this._sharingState?.geo) {
      let geoPermission = permissions.find(perm => perm.id === "geo");
      if (geoPermission) {
        geoPermission.sharingState = true;
      } else {
        permissions.push({
          id: "geo",
          state: SitePermissions.ALLOW,
          scope: SitePermissions.SCOPE_REQUEST,
          sharingState: true,
        });
      }
    }

    if (this._sharingState?.xr) {
      let xrPermission = permissions.find(perm => perm.id === "xr");
      if (xrPermission) {
        xrPermission.sharingState = true;
      } else {
        permissions.push({
          id: "xr",
          state: SitePermissions.ALLOW,
          scope: SitePermissions.SCOPE_REQUEST,
          sharingState: true,
        });
      }
    }

    if (this._sharingState?.webRTC) {
      let webrtcState = this._sharingState.webRTC;
      // If WebRTC device or screen permissions are in use, we need to find
      // the associated permission item to set the sharingState field.
      for (let id of ["camera", "microphone", "screen"]) {
        if (webrtcState[id]) {
          let found = false;
          for (let permission of permissions) {
            if (permission.id != id) {
              continue;
            }
            found = true;
            permission.sharingState = webrtcState[id];
            break;
          }
          if (!found) {
            // If the permission item we were looking for doesn't exist,
            // the user has temporarily allowed sharing and we need to add
            // an item in the permissions array to reflect this.
            permissions.push({
              id,
              state: SitePermissions.ALLOW,
              scope: SitePermissions.SCOPE_REQUEST,
              sharingState: webrtcState[id],
            });
          }
        }
      }
    }

    let totalBlockedPopups = gBrowser.selectedBrowser.popupBlocker.getBlockedPopupCount();
    let hasBlockedPopupIndicator = false;
    for (let permission of permissions) {
      let [id, key] = permission.id.split(SitePermissions.PERM_KEY_DELIMITER);

      if (id == "storage-access") {
        // Ignore storage access permissions here, they are made visible inside
        // the Content Blocking UI.
        continue;
      }

      let item;
      let anchor =
        this._permissionList.querySelector(`[anchorfor="${id}"]`) ||
        this._defaultPermissionAnchor;

      if (id == "open-protocol-handler") {
        let permContainer = this._createProtocolHandlerPermissionItem(
          permission,
          key
        );
        if (permContainer) {
          anchor.appendChild(permContainer);
        }
      } else {
        item = this._createPermissionItem({
          permission,
          isContainer: id == "geo" || id == "xr",
          nowrapLabel: id == "3rdPartyStorage",
        });

        if (!item) {
          continue;
        }
        anchor.appendChild(item);
      }

      if (id == "popup" && totalBlockedPopups) {
        this._createBlockedPopupIndicator(totalBlockedPopups);
        hasBlockedPopupIndicator = true;
      } else if (id == "geo" && permission.state === SitePermissions.ALLOW) {
        this._createGeoLocationLastAccessIndicator();
      }
    }

    if (totalBlockedPopups && !hasBlockedPopupIndicator) {
      let permission = {
        id: "popup",
        state: SitePermissions.getDefault("popup"),
        scope: SitePermissions.SCOPE_PERSISTENT,
      };
      let item = this._createPermissionItem({ permission });
      this._defaultPermissionAnchor.appendChild(item);
      this._createBlockedPopupIndicator(totalBlockedPopups);
    }

    PanelView.forNode(
      this._permissionPopupMainView
    ).descriptionHeightWorkaround();
  },

  /**
   * Creates a permission item based on the supplied options and returns it.
   * It is up to the caller to actually insert the element somewhere.
   *
   * @param permission - An object containing information representing the
   *                     permission, typically obtained via SitePermissions.jsm
   * @param isContainer - If true, the permission item will be added to a vbox
   *                      and the vbox will be returned.
   * @param permClearButton - Whether to show an "x" button to clear the permission
   * @param showStateLabel - Whether to show a label indicating the current status
   *                         of the permission e.g. "Temporary Allowed"
   * @param idNoSuffix - Some permission types have additional information suffixed
   *                     to the ID - callers can pass the unsuffixed ID via this
   *                     parameter to indicate the permission type manually.
   * @param nowrapLabel - Whether to prevent the permission item's label from
   *                      wrapping its text content. This allows styling text-overflow
   *                      and is useful for e.g. 3rdPartyStorage permissions whose
   *                      labels are origins - which could be of any length.
   */
  _createPermissionItem({
    permission,
    isContainer = false,
    permClearButton = true,
    showStateLabel = true,
    idNoSuffix = permission.id,
    nowrapLabel = false,
  }) {
    let container = document.createXULElement("hbox");
    container.setAttribute("class", "permission-popup-permission-item");
    container.setAttribute("align", "center");
    container.setAttribute("role", "group");

    let img = document.createXULElement("image");
    img.classList.add("permission-popup-permission-icon", idNoSuffix + "-icon");
    if (
      permission.state == SitePermissions.BLOCK ||
      permission.state == SitePermissions.AUTOPLAY_BLOCKED_ALL
    ) {
      img.classList.add("blocked-permission-icon");
    }

    if (
      permission.sharingState ==
        Ci.nsIMediaManagerService.STATE_CAPTURE_ENABLED ||
      (idNoSuffix == "screen" &&
        permission.sharingState &&
        !permission.sharingState.includes("Paused"))
    ) {
      img.classList.add("in-use");

      // Synchronize permission panel and identity block blinking animations.
      window
        .promiseDocumentFlushed(() => {
          let sharingIconBlink = this._webRTCSharingIcon.getAnimations()[0];
          let imgBlink = img.getAnimations()[0];
          return [sharingIconBlink, imgBlink];
        })
        .then(([sharingIconBlink, imgBlink]) => {
          if (sharingIconBlink && imgBlink) {
            imgBlink.startTime = sharingIconBlink.startTime;
          }
        });
    }

    let nameLabel = document.createXULElement("label");
    nameLabel.setAttribute("flex", "1");
    nameLabel.setAttribute("class", "permission-popup-permission-label");
    let label = SitePermissions.getPermissionLabel(idNoSuffix);
    if (label === null) {
      return null;
    }
    if (nowrapLabel) {
      nameLabel.setAttribute("value", label);
      nameLabel.setAttribute("tooltiptext", label);
      nameLabel.setAttribute("crop", "end");
    } else {
      nameLabel.textContent = label;
    }
    let nameLabelId = "permission-popup-permission-label-" + idNoSuffix;
    nameLabel.setAttribute("id", nameLabelId);

    let isPolicyPermission = [
      SitePermissions.SCOPE_POLICY,
      SitePermissions.SCOPE_GLOBAL,
    ].includes(permission.scope);

    if (
      (idNoSuffix == "popup" && !isPolicyPermission) ||
      idNoSuffix == "autoplay-media"
    ) {
      let menulist = document.createXULElement("menulist");
      let menupopup = document.createXULElement("menupopup");
      let block = document.createXULElement("vbox");
      block.setAttribute("id", "permission-popup-container");
      block.setAttribute("class", "permission-popup-permission-item-container");
      menulist.setAttribute("sizetopopup", "none");
      menulist.setAttribute("id", "permission-popup-menulist");

      for (let state of SitePermissions.getAvailableStates(idNoSuffix)) {
        let menuitem = document.createXULElement("menuitem");
        // We need to correctly display the default/unknown state, which has its
        // own integer value (0) but represents one of the other states.
        if (state == SitePermissions.getDefault(idNoSuffix)) {
          menuitem.setAttribute("value", "0");
        } else {
          menuitem.setAttribute("value", state);
        }

        menuitem.setAttribute(
          "label",
          SitePermissions.getMultichoiceStateLabel(idNoSuffix, state)
        );
        menupopup.appendChild(menuitem);
      }

      menulist.appendChild(menupopup);

      if (permission.state == SitePermissions.getDefault(idNoSuffix)) {
        menulist.value = "0";
      } else {
        menulist.value = permission.state;
      }

      // Avoiding listening to the "select" event on purpose. See Bug 1404262.
      menulist.addEventListener("command", () => {
        SitePermissions.setForPrincipal(
          gBrowser.contentPrincipal,
          idNoSuffix,
          menulist.selectedItem.value
        );
      });

      container.appendChild(img);
      container.appendChild(nameLabel);
      container.appendChild(menulist);
      container.setAttribute("aria-labelledby", nameLabelId);
      block.appendChild(container);

      return block;
    }

    container.appendChild(img);
    container.appendChild(nameLabel);
    let labelledBy = nameLabelId;

    if (showStateLabel) {
      let stateLabel = this._createStateLabel(permission, idNoSuffix);
      container.appendChild(stateLabel);
      labelledBy += " " + stateLabel.id;
    }

    container.setAttribute("aria-labelledby", labelledBy);

    /* We return the permission item here without a remove button if the permission is a
       SCOPE_POLICY or SCOPE_GLOBAL permission. Policy permissions cannot be
       removed/changed for the duration of the browser session. */
    if (isPolicyPermission) {
      return container;
    }

    if (isContainer) {
      let block = document.createXULElement("vbox");
      block.setAttribute("id", "permission-popup-" + idNoSuffix + "-container");
      block.setAttribute("class", "permission-popup-permission-item-container");

      if (permClearButton) {
        let button = this._createPermissionClearButton(permission, block);
        container.appendChild(button);
      }

      block.appendChild(container);
      return block;
    }

    if (permClearButton) {
      let button = this._createPermissionClearButton(permission, container);
      container.appendChild(button);
    }

    return container;
  },

  _createStateLabel(aPermission, idNoSuffix) {
    let label = document.createXULElement("label");
    label.setAttribute("flex", "1");
    label.setAttribute("class", "permission-popup-permission-state-label");
    let labelId = "permission-popup-permission-state-label-" + idNoSuffix;
    label.setAttribute("id", labelId);
    let { state, scope } = aPermission;
    // If the user did not permanently allow this device but it is currently
    // used, set the variables to display a "temporarily allowed" info.
    if (state != SitePermissions.ALLOW && aPermission.sharingState) {
      state = SitePermissions.ALLOW;
      scope = SitePermissions.SCOPE_REQUEST;
    }
    label.textContent = SitePermissions.getCurrentStateLabel(
      state,
      idNoSuffix,
      scope
    );
    return label;
  },

  _removePermPersistentAllow(principal, id) {
    let perm = SitePermissions.getForPrincipal(principal, id);
    if (
      perm.state == SitePermissions.ALLOW &&
      perm.scope == SitePermissions.SCOPE_PERSISTENT
    ) {
      SitePermissions.removeFromPrincipal(principal, id);
    }
  },

  _createPermissionClearButton(
    aPermission,
    container,
    clearCallback = () => {}
  ) {
    let button = document.createXULElement("button");
    button.setAttribute("class", "permission-popup-permission-remove-button");
    let tooltiptext = gNavigatorBundle.getString("permissions.remove.tooltip");
    button.setAttribute("tooltiptext", tooltiptext);
    button.addEventListener("command", () => {
      let browser = gBrowser.selectedBrowser;
      container.remove();
      if (aPermission.sharingState) {
        if (aPermission.id === "geo" || aPermission.id === "xr") {
          let origins = browser.getDevicePermissionOrigins(aPermission.id);
          for (let origin of origins) {
            let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
              origin
            );
            this._removePermPersistentAllow(principal, aPermission.id);
          }
          origins.clear();
        } else if (
          ["camera", "microphone", "screen"].includes(aPermission.id)
        ) {
          let windowId = this._sharingState.webRTC.windowId;
          if (aPermission.id == "screen") {
            windowId = "screen:" + windowId;
          } else {
            // If we set persistent permissions or the sharing has
            // started due to existing persistent permissions, we need
            // to handle removing these even for frames with different hostnames.
            let origins = browser.getDevicePermissionOrigins("webrtc");
            for (let origin of origins) {
              // It's not possible to stop sharing one of camera/microphone
              // without the other.
              let principal;
              for (let id of ["camera", "microphone"]) {
                if (this._sharingState.webRTC[id]) {
                  if (!principal) {
                    principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
                      origin
                    );
                  }
                  this._removePermPersistentAllow(principal, id);
                }
              }
            }
          }

          let bc = this._sharingState.webRTC.browsingContext;
          bc.currentWindowGlobal
            .getActor("WebRTC")
            .sendAsyncMessage("webrtc:StopSharing", windowId);
          webrtcUI.forgetActivePermissionsFromBrowser(gBrowser.selectedBrowser);
        }
      }
      SitePermissions.removeFromPrincipal(
        gBrowser.contentPrincipal,
        aPermission.id,
        browser
      );

      this._permissionReloadHint.hidden = false;
      PanelView.forNode(
        this._permissionPopupMainView
      ).descriptionHeightWorkaround();

      if (aPermission.id === "geo") {
        gBrowser.updateBrowserSharing(browser, { geo: false });
      } else if (aPermission.id === "xr") {
        gBrowser.updateBrowserSharing(browser, { xr: false });
      }

      clearCallback();
    });

    return button;
  },

  _getGeoLocationLastAccess() {
    return new Promise(resolve => {
      let lastAccess = null;
      ContentPrefService2.getByDomainAndName(
        gBrowser.currentURI.spec,
        "permissions.geoLocation.lastAccess",
        gBrowser.selectedBrowser.loadContext,
        {
          handleResult(pref) {
            lastAccess = pref.value;
          },
          handleCompletion() {
            resolve(lastAccess);
          },
        }
      );
    });
  },

  async _createGeoLocationLastAccessIndicator() {
    let lastAccessStr = await this._getGeoLocationLastAccess();
    let geoContainer = document.getElementById(
      "permission-popup-geo-container"
    );

    // Check whether geoContainer still exists.
    // We are async, the identity popup could have been closed already.
    // Also check if it is already populated with a time label.
    // This can happen if we update the permission panel multiple times in a
    // short timeframe.
    if (
      lastAccessStr == null ||
      !geoContainer ||
      document.getElementById("geo-access-indicator-item")
    ) {
      return;
    }
    let lastAccess = new Date(lastAccessStr);
    if (isNaN(lastAccess)) {
      Cu.reportError("Invalid timestamp for last geolocation access");
      return;
    }

    let icon = document.createXULElement("image");
    icon.setAttribute("class", "popup-subitem");

    let indicator = document.createXULElement("hbox");
    indicator.setAttribute("class", "permission-popup-permission-item");
    indicator.setAttribute("align", "center");
    indicator.setAttribute("id", "geo-access-indicator-item");

    let timeFormat = new Services.intl.RelativeTimeFormat(undefined, {});

    let text = document.createXULElement("label");
    text.setAttribute("flex", "1");
    text.setAttribute("class", "permission-popup-permission-label");

    text.textContent = gNavigatorBundle.getFormattedString(
      "geolocationLastAccessIndicatorText",
      [timeFormat.formatBestUnit(lastAccess)]
    );

    indicator.appendChild(icon);
    indicator.appendChild(text);

    geoContainer.appendChild(indicator);
  },

  _createProtocolHandlerPermissionItem(permission, key) {
    let container = document.getElementById(
      "permission-popup-open-protocol-handler-container"
    );
    let initialCall;

    if (!container) {
      // First open-protocol-handler permission, create container.
      container = this._createPermissionItem({
        permission,
        isContainer: true,
        permClearButton: false,
        showStateLabel: false,
        idNoSuffix: "open-protocol-handler",
      });
      initialCall = true;
    }

    let icon = document.createXULElement("image");
    icon.setAttribute("class", "popup-subitem-no-arrow");

    let item = document.createXULElement("hbox");
    item.setAttribute("class", "permission-popup-permission-item");
    item.setAttribute("align", "center");

    let text = document.createXULElement("label");
    text.setAttribute("flex", "1");
    text.setAttribute("class", "permission-popup-permission-label-subitem");

    text.textContent = gNavigatorBundle.getFormattedString(
      "openProtocolHandlerPermissionEntryLabel",
      [key]
    );

    let stateLabel = this._createStateLabel(
      permission,
      "open-protocol-handler"
    );

    item.appendChild(text);
    item.appendChild(stateLabel);

    let button = this._createPermissionClearButton(permission, item, () => {
      // When we're clearing the last open-protocol-handler permission, clean up
      // the empty container.
      // (<= 1 because the heading item is also a child of the container)
      if (container.childElementCount <= 1) {
        container.remove();
      }
    });
    item.appendChild(button);

    container.appendChild(item);

    // If container already exists in permission list, don't return it again.
    return initialCall && container;
  },

  _createBlockedPopupIndicator(aTotalBlockedPopups) {
    let indicator = document.createXULElement("hbox");
    indicator.setAttribute("class", "permission-popup-permission-item");
    indicator.setAttribute("align", "center");
    indicator.setAttribute("id", "blocked-popup-indicator-item");

    let icon = document.createXULElement("image");
    icon.setAttribute("class", "popup-subitem");

    let text = document.createXULElement("label", { is: "text-link" });
    text.setAttribute("flex", "1");
    text.setAttribute("class", "permission-popup-permission-label");

    let messageBase = gNavigatorBundle.getString(
      "popupShowBlockedPopupsIndicatorText"
    );
    let message = PluralForm.get(aTotalBlockedPopups, messageBase).replace(
      "#1",
      aTotalBlockedPopups
    );
    text.textContent = message;

    text.addEventListener("click", () => {
      gBrowser.selectedBrowser.popupBlocker.unblockAllPopups();
    });

    indicator.appendChild(icon);
    indicator.appendChild(text);

    document
      .getElementById("permission-popup-container")
      .appendChild(indicator);
  },
};
