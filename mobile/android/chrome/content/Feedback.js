/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var Feedback = {

  get _feedbackURL() {
    delete this._feedbackURL;
    return this._feedbackURL = Services.prefs.getCharPref("app.feedbackURL");
  },

  observe: function(aMessage, aTopic, aData) {
    if (aTopic !== "Feedback:Show") {
      return;
    }

    // Don't prompt for feedback in distribution builds.
    try {
      Services.prefs.getCharPref("distribution.id");
      return;
    } catch (e) {}

    let url = this._feedbackURL + "?source=feedback-prompt";
    let browser = BrowserApp.selectOrAddTab(url, { parentId: BrowserApp.selectedTab.id }).browser;
    browser.addEventListener("FeedbackClose", this, false, true);
    browser.addEventListener("FeedbackMaybeLater", this, false, true);
    browser.addEventListener("FeedbackOpenPlay", this, false, true);
  },

  handleEvent: function(event) {
    if (!this._isAllowed(event.target)) {
      return;
    }

    switch (event.type) {
      case "FeedbackClose":
        // Do nothing.
        break;

      case "FeedbackMaybeLater":
        Messaging.sendRequest({ type: "Feedback:MaybeLater" });
        break;

      case "FeedbackOpenPlay":
        Messaging.sendRequest({ type: "Feedback:OpenPlayStore" });
        break;

    }

    let win = event.target.ownerDocument.defaultView.top;
    BrowserApp.closeTab(BrowserApp.getTabForWindow(win));
  },

  _isAllowed: function(node) {
    let uri = node.ownerDocument.documentURIObject;
    let feedbackURI = Services.io.newURI(this._feedbackURL, null, null);
    return uri.prePath === feedbackURI.prePath;
  }
};
