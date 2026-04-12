$(document).ready(function () {
  // Perform initial translation setup
  performTranslation();

  // Check if the effect engine is enabled
  const EFFECTENGINE_ENABLED = (jQuery.inArray("effectengine", globalThis.serverInfo.services) !== -1);

  const DEFAULT_EFFECTS_COLOR = '#B500FF';
  const BG_PRIORITY = 254;
  const inputSourcesWithOwnerLabel = new Set(["GRABBER", "V4L", "AUDIO"]);
  const inputSourcesWithoutOwnerLabel = new Set(["BOBLIGHTSERVER", "FLATBUFSERVER", "PROTOSERVER"]);
  const COLOR_INPUT_SELECTOR = String.raw`#root\[colorEffects\]\[color\]`;
  const IMAGE_INPUT_SELECTOR = String.raw`#root\[colorEffects\]\[image\]`;
  const IMAGE_FIELD_SELECTOR = '[data-schemapath="root.colorEffects.image"]';
  const EFFECTS_SELECT_INPUT_SELECTOR = String.raw`#root\[colorEffects\]\[effect\]`;
  const EFFECTS_DURATION_INPUT_SELECTOR = String.raw`#root\[colorEffects\]\[duration\]`;
  const EFFECTS_DURATION_ENDLESS_HINT_ID = 'duration-endless-hint';
  const COLOR_RERUN_BUTTON_ID = 'btn_rerun_colorEffects_color';
  const IMAGE_RERUN_BUTTON_ID = 'btn_rerun_colorEffects_upload';
  const EFFECTS_RERUN_BUTTON_ID = 'btn_rerun_colorEffects_effect';
  const COLOR_RERUN_WRAPPER_ID = 'colorEffects-color-rerun-wrap';
  const IMAGE_RERUN_WRAPPER_ID = 'colorEffects-image-rerun-wrap';
  const EFFECTS_RERUN_WRAPPER_ID = 'colorEffects-effect-rerun-wrap';

  const editors = {}; // Store JSON editors in a structured way
  const initialEffectsColor = getStorage('remoteColorEffectsColor') || DEFAULT_EFFECTS_COLOR;
  let colorEffects = {
    color: initialEffectsColor,
    colorRGB: hexToRgb(initialEffectsColor),
    effect: "",
    image: "",
    duration_s: Number(getStorage('remoteColorEffectsDuration')) || 0, // 0 = Endless
    lastImgData: ""
  };
  const parsePriorityOrigin = (priorityEntry) => {
    const origin = priorityEntry.origin ? priorityEntry.origin : "System";
    const [originName, ip] = origin.split("@");
    return { originName, ip };
  };


  // Update the list of Hyperion instances
  updateHyperionInstanceListing();

  initializeUI();
  initComponents();
  updateInputSelect();
  updateVideoMode();

  if (isCurrentInstanceRunning()) {
    setupEventListenersForUpdates();
  }

  removeOverlay();

  function initializeUI() {

    setupSourceSelection();
    setupColorEditor();
    setupImageProcessingEditor();
    setupChannelAdjustmentEditor();

    // Create introduction hints if the help option is enabled
    if (globalThis.showOptHelp) {
      createHint("intro", $.i18n('remote_components_intro', $.i18n('remote_losthint')), "comp_intro");
      createHint("intro", $.i18n('remote_videoMode_intro', $.i18n('remote_losthint')), "videomode_intro");
    }
  }

  /////////////////////////////////
  /// Color and Effect Management
  /////////////////////////////////

  function setupColorEditor() {

    createSection('colorEffects', $.i18n("remote_color_label"), '', 'fa-wifi', 'remote_color_intro', {}, 'card-default', '');
    const colorEffectsSubmitButton = document.getElementById('btn_submit_colorEffects');
    if (colorEffectsSubmitButton) {
      colorEffectsSubmitButton.dataset.i18n = 'remote_color_button_reset';
      colorEffectsSubmitButton.setAttribute('type', 'button');
      colorEffectsSubmitButton.innerHTML = `<i class="fa fa-fw fa-undo"></i>${$.i18n('remote_color_button_reset')}`;
      colorEffectsSubmitButton.disabled = !isCurrentInstanceRunning();
    }

    updateColorEffectsEditor();
  }

  function getColorEffectsEditorSchema() {
    let colorEffectsSchema = {
      colorEffects: {
        "type": "object",
        "title": "edt_conf_bge_heading_title",
        "options": {
          "titleHidden": false
        },
        "properties": {
          "color": {
            "type": "string",
            "title": "remote_color_label_color",
            "format": "color",
            "propertyOrder": 1
          },
          "image": {
            "type": "string",
            "title": "remote_effects_label_picture",
            "media": {
              "binaryEncoding": "base64"
            },
            "options": {
              "infoText": $.i18n('remote__effects_picture_infoText'),
            },
            "propertyOrder": 3
          },
          "duration": {
            "type": "integer",
            "title": "remote_input_duration",
            "default": 0,
            "minimum": 0,
            "append": "edt_append_s",
            "propertyOrder": 4
          }
        }
      }
    };

    if (EFFECTENGINE_ENABLED) {
      const effectNames = [$.i18n('edt_conf_enum_please_select'), ...getAvailableEffectNames()];
      colorEffectsSchema.colorEffects.properties.effect = {
        "type": "string",
        "format": "choices",
        "title": "remote_effects_label_effects",
        "enum": effectNames,
        "options": {
          "enum_titles": effectNames,
          "choices": {
            "placeholder": true,
            "placeholderValue": $.i18n('edt_conf_enum_please_select'),
          }
        },
        "propertyOrder": 2
      };
    }

    return colorEffectsSchema;
  }

  function updateColorEffectsEditor() {

    if (editors['colorEffects']) {
      editors['colorEffects'].destroy();
      delete editors['colorEffects'];
    }

    const startval = {
      colorEffects: {
        color: colorEffects.color,
        image: colorEffects.image,
        duration: colorEffects.duration_s
      }
    };

    if (EFFECTENGINE_ENABLED) {
      startval.colorEffects.effect = colorEffects.effect;
    }

    createEditor(editors, 'colorEffects', getColorEffectsEditorSchema(), handleColorEffectsChange, {
      bindDefaultChange: false,
      bindSubmit: true,
      onSubmit: resetColorEffectsEditor,
      startval: startval
    });
  }

  function handleColorEffectsChange(editor) {

    editor.on('ready', () => {
      if (!isCurrentInstanceRunning()) {
        editor.disable();
        return;
      }

      const duration = editor.getEditor("root.colorEffects.duration").getValue();
      colorEffects.effect = editor.getEditor("root.colorEffects.effect")?.getValue() || "";
      syncDurationValue(duration);

      appendRerunColorButton(editor);
      appendRerunUploadButton(editor);
      appendRerunEffectButton(editor);
      updateUploadActionButtonsVisibility();
      updateEffectActionButtonsVisibility();
    });

    editor.watch('root.colorEffects.color', () => {
      if (!editor.ready) return;
      const color = editor.getEditor("root.colorEffects.color").getValue();
      colorEffects.color = color;
      debugMessage("Color changed to: " + JSON.stringify(color));

      colorEffects.colorRGB = hexToRgb(color);
      if (!colorEffects.colorRGB) {
        return;
      }
      setStorage('remoteColorEffectsColor', color);
      requestSetColor(colorEffects.colorRGB.r, colorEffects.colorRGB.g, colorEffects.colorRGB.b, colorEffects.duration_s);
    });

    editor.watch('root.colorEffects.effect', () => {
      if (!editor.ready) return;
      const effect = editor.getEditor("root.colorEffects.effect").getValue();
      colorEffects.effect = effect;
      updateEffectActionButtonsVisibility();

      debugMessage("Effect changed to: " + effect);
      requestPlayEffect(effect, colorEffects.duration_s);
    });

    editor.watch('root.colorEffects.image', () => {
      if (!editor.ready) return;
      const uploadedFileData = editor.getEditor("root.colorEffects.image").getValue();
      debugMessage("Uploaded file data changed to: " + JSON.stringify(uploadedFileData));

      if (!uploadedFileData?.includes(",")) {
        colorEffects.lastImgData = "";
        updateUploadActionButtonsVisibility();
        return;
      }

      const [type, data] = uploadedFileData.split(",");
      if (!(type.includes("image") && type.includes("base64"))) {
        colorEffects.lastImgData = "";
        updateUploadActionButtonsVisibility();
        return;
      }

      colorEffects.lastImgData = data;
      updateUploadActionButtonsVisibility();
      requestSetImage(colorEffects.lastImgData, colorEffects.duration_s);
    });

    editor.watch('root.colorEffects.duration', () => {
      if (!editor.ready) return;
      const duration = editor.getEditor("root.colorEffects.duration").getValue();
      debugMessage("Duration changed to: " + duration);
      syncDurationValue(duration);
    });
  }

  function appendRerunEffectButton(editor) {
    if (!EFFECTENGINE_ENABLED || !editor?.ready) {
      return;
    }

    const effectInput = $(EFFECTS_SELECT_INPUT_SELECTOR);
    if (effectInput.length === 0 || document.getElementById(EFFECTS_RERUN_BUTTON_ID)) {
      return;
    }

    const rerunButton = $('<button/>', {
      id: EFFECTS_RERUN_BUTTON_ID,
      type: 'button',
      class: 'btn btn-secondary btn-sm'
    }).html('<i class="fa fa-repeat"></i>');

    rerunButton.css({
      display: 'inline-flex',
      alignItems: 'center',
      justifyContent: 'center',
      whiteSpace: 'nowrap',
      flex: '0 0 auto',
      margin: '0'
    });

    rerunButton.on('click', () => {
      const selectedEffect = editor.getEditor('root.colorEffects.effect')?.getValue();
      if (!selectedEffect || selectedEffect === $.i18n('edt_conf_enum_please_select')) {
        return;
      }

      colorEffects.effect = selectedEffect;
      requestPlayEffect(selectedEffect, colorEffects.duration_s);
    });

    const choicesContainer = effectInput.closest('.choices');
    if (choicesContainer.length > 0) {
      let wrapper = $(`#${EFFECTS_RERUN_WRAPPER_ID}`);
      if (wrapper.length === 0) {
        choicesContainer.wrap(`<div id="${EFFECTS_RERUN_WRAPPER_ID}" class="d-flex align-items-center gap-2"></div>`);
        wrapper = $(`#${EFFECTS_RERUN_WRAPPER_ID}`);
        wrapper.css({
          display: 'flex',
          alignItems: 'stretch',
          gap: '0.5rem',
          width: '100%'
        });

        choicesContainer.css({
          flex: '1 1 auto',
          minWidth: '0',
          marginBottom: '0'
        });
      }

      wrapper.append(rerunButton);
      updateEffectActionButtonsVisibility();
      return;
    }

    effectInput.after(rerunButton);
    updateEffectActionButtonsVisibility();
  }

  function updateEffectActionButtonsVisibility() {
    const selectedEffect = colorEffects.effect;
    const hasEffect = Boolean(selectedEffect) && selectedEffect !== $.i18n('edt_conf_enum_please_select');
    const rerunEffectButton = $(`#${EFFECTS_RERUN_BUTTON_ID}`);
    if (rerunEffectButton.length > 0) {
      rerunEffectButton.toggle(hasEffect);
    }
  }

  function appendRerunColorButton(editor) {
    if (!editor?.ready) {
      return;
    }

    const colorInput = $(COLOR_INPUT_SELECTOR);
    if (colorInput.length === 0 || document.getElementById(COLOR_RERUN_BUTTON_ID)) {
      return;
    }

    const rerunButton = $('<button/>', {
      id: COLOR_RERUN_BUTTON_ID,
      type: 'button',
      class: 'btn btn-secondary btn-sm',
      title: $.i18n('remote_color_label_color')
    }).html('<i class="fa fa-repeat"></i>');

    rerunButton.css({
      display: 'inline-flex',
      alignItems: 'center',
      justifyContent: 'center',
      whiteSpace: 'nowrap',
      flex: '0 0 auto'
    });

    rerunButton.on('click', () => {
      const selectedColor = editor.getEditor('root.colorEffects.color')?.getValue();
      const colorRgb = hexToRgb(selectedColor);
      if (!colorRgb) {
        return;
      }

      colorEffects.color = selectedColor;
      colorEffects.colorRGB = colorRgb;
      requestSetColor(colorRgb.r, colorRgb.g, colorRgb.b, colorEffects.duration_s);
    });

    let wrapper = $(`#${COLOR_RERUN_WRAPPER_ID}`);
    if (wrapper.length === 0) {
      colorInput.wrap(`<div id="${COLOR_RERUN_WRAPPER_ID}" class="d-flex align-items-center gap-2"></div>`);
      wrapper = $(`#${COLOR_RERUN_WRAPPER_ID}`);
      wrapper.css({
        display: 'flex',
        alignItems: 'center',
        gap: '0.5rem',
        width: '100%'
      });

      colorInput.css({
        flex: '1 1 auto',
        minWidth: '0',
        marginBottom: '0'
      });
    }

    wrapper.append(rerunButton);
  }

  function appendRerunUploadButton(editor) {
    if (!editor?.ready) {
      return;
    }

    const imageField = $(IMAGE_FIELD_SELECTOR).first();
    const imageInput = $(IMAGE_INPUT_SELECTOR);
    if ((imageField.length === 0 && imageInput.length === 0) || document.getElementById(IMAGE_RERUN_BUTTON_ID)) {
      return;
    }

    const rerunButton = $('<button/>', {
      id: IMAGE_RERUN_BUTTON_ID,
      type: 'button',
      class: 'btn btn-secondary btn-sm',
      title: $.i18n('remote_effects_label_picture')
    }).html('<i class="fa fa-repeat"></i>');

    rerunButton.css({
      display: 'inline-flex',
      alignItems: 'center',
      justifyContent: 'center',
      whiteSpace: 'nowrap',
      flex: '0 0 auto'
    });

    rerunButton.on('click', () => {
      if (!colorEffects.lastImgData) {
        return;
      }

      requestSetImage(colorEffects.lastImgData, colorEffects.duration_s);
    });

    const parentGroup = imageField.length > 0 ? imageField.find('.form-group').first() : imageInput.closest('.form-group');
    const uploadButton = parentGroup.find('.json-editor-btn-upload').first();
    if (uploadButton.length > 0) {
      let wrapper = $(`#${IMAGE_RERUN_WRAPPER_ID}`);
      if (wrapper.length === 0) {
        wrapper = $('<div/>', {
          id: IMAGE_RERUN_WRAPPER_ID,
          class: 'd-flex align-items-center'
        });
        uploadButton.after(wrapper);
        wrapper.append(uploadButton);
      } else {
        wrapper = $(`#${IMAGE_RERUN_WRAPPER_ID}`);
        wrapper.append(uploadButton);
      }

      wrapper.css({
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        width: '100%',
        marginTop: '0.5rem'
      });

      uploadButton.css({
        margin: '0',
        flex: '0 0 auto'
      });

      wrapper.append(rerunButton);
      updateUploadActionButtonsVisibility();
      return;
    }

    imageInput.after(rerunButton);
    updateUploadActionButtonsVisibility();
  }

  function updateUploadActionButtonsVisibility() {
    const hasImageData = Boolean(colorEffects.lastImgData);
    const rerunUploadButton = $(`#${IMAGE_RERUN_BUTTON_ID}`);
    if (rerunUploadButton.length > 0) {
      rerunUploadButton.toggle(hasImageData);
    }
  }

  function resetColorEffectsEditor() {
    colorEffects.effect = "";
    colorEffects.image = "";
    colorEffects.lastImgData = "";
    updateColorEffectsEditor();
  }


  function syncDurationValue(value) {
    setStorage('remoteColorEffectsDuration', value);
    colorEffects.duration_s = value;
    updateDurationPlaceholder();
  }

  function getDurationInput() {
    return $(EFFECTS_DURATION_INPUT_SELECTOR);
  }

  function ensureDurationHintElement(durationInput) {
    if (durationInput.length === 0) {
      return $();
    }

    const parent = durationInput.parent();
    parent.css('position', 'relative');

    let hint = parent.find(`#${EFFECTS_DURATION_ENDLESS_HINT_ID}`);
    if (hint.length === 0) {
      hint = $('<label/>', {
        id: EFFECTS_DURATION_ENDLESS_HINT_ID,
        class: 'text-muted'
      }).hide();

      hint.css({
        position: 'absolute',
        left: '0.75rem',
        top: '50%',
        transform: 'translateY(-50%)',
        pointerEvents: 'none',
        margin: '0',
        fontWeight: '400'
      });

      if (durationInput.attr('id')) {
        hint.attr('for', durationInput.attr('id'));
      }

      parent.append(hint);
    }

    return hint;
  }

  function ensureDurationDisplayStyle() {
    if ($('#duration-endless-style').length > 0) {
      return;
    }

    const style = String.raw`
      #root\[colorEffects\]\[duration\].duration-endless-display {
        color: transparent;
        text-shadow: none;
        caret-color: currentColor;
      }
    `;
    $('<style/>', {
      id: 'duration-endless-style',
      text: style
    }).appendTo('head');
  }

  // Keep numeric value and overlay "Endless" text when value is 0
  function updateDurationPlaceholder() {
    const durationInput = getDurationInput();
    if (durationInput.length === 0) {
      return;
    }

    const durationValue = Number(durationInput.val());

    const isEndless = !Number.isNaN(durationValue) && durationValue === 0;
    const durationHint = ensureDurationHintElement(durationInput);
    ensureDurationDisplayStyle();

    if (isEndless) {
      durationInput.addClass('duration-endless-display');
      durationHint.text($.i18n('remote_input_duration_endless')).show();
    } else {
      durationInput.removeClass('duration-endless-display');
      durationHint.text("").hide();
    }
  }

  /////////////////////////////////
  /// Image Processing Management
  /////////////////////////////////

  function setupImageProcessingEditor() {

    createSection('imageProcessing', $.i18n("remote_maptype_label"), '', 'fa-wifi', 'remote_maptype_intro', {}, 'card-default', '');

    const imageProcessingSubmitButton = document.getElementById('btn_submit_imageProcessing');
    const imageProcessingDefaultsButtonId = 'btn_submit_imageProcessingDefaults';
    let imageProcessingDefaultsButton = document.getElementById(imageProcessingDefaultsButtonId);
    if (imageProcessingSubmitButton && !document.getElementById(imageProcessingDefaultsButtonId)) {
      const defaultsButton = document.createElement('button');
      defaultsButton.className = 'btn btn-primary me-2';
      defaultsButton.id = imageProcessingDefaultsButtonId;
      defaultsButton.type = 'button';
      defaultsButton.innerHTML = '<i class="fa fa-fw fa-undo"></i><span data-i18n="general_btn_reset">' + $.i18n('general_btn_reset') + '</span>';
      defaultsButton.addEventListener('click', resetToImageProcessingConfig);
      imageProcessingSubmitButton.parentNode.insertBefore(defaultsButton, imageProcessingSubmitButton);
      imageProcessingDefaultsButton = defaultsButton;
    }

    applyResponsiveActionButtonsLayout(imageProcessingSubmitButton, imageProcessingDefaultsButton);

    const startval = {
      imageProcessing: globalThis.serverConfig.color.imageProcessing
        ? globalThis.serverConfig.color.imageProcessing
        : {}
    };

    console.log("Starting ImageProcessing Editor with value: " + JSON.stringify(startval));
    createEditor(editors, 'imageProcessing', 'imageProcessing', handleImageProcessingChange, {
      bindDefaultChange: false,
      bindSubmit: true,
      onSubmit: saveImageProcessingConfig,
      startval: startval
    });

    $('#btn_submit_imageProcessing').prop('disabled', !isCurrentInstanceRunning());
    $('#btn_submit_imageProcessingDefaults').prop('disabled', !isCurrentInstanceRunning());
  }

  function resetToImageProcessingConfig() {
    console.log("Setting Image Processing config values...");
    editors['imageProcessing'].setValue({
      imageProcessing: globalThis.serverConfig.color.imageProcessing
        ? globalThis.serverConfig.color.imageProcessing
        : {}
    });
  }

  function saveImageProcessingConfig() {
    if (editors['imageProcessing'].ready) {
      console.log("Submitting ImageProcessing configuration: " + JSON.stringify(editors['imageProcessing'].getValue()));
      requestWriteConfig(editors['imageProcessing'].getValue());
    }
  }

  function updateLedMapping() {
    const mapping = globalThis.serverInfo.imageToLedMappingType;
    if (editors['imageProcessing']?.ready) {
      const currentValue = editors['imageProcessing'].getValue().imageProcessing?.imageToLedMappingType;
      if (currentValue !== mapping) {
        editors['imageProcessing'].setValue({ imageProcessing: { imageToLedMappingType: mapping } });
      }
    }
  }

  /////////////////////////////////
  /// Channel Adjustment Management
  /////////////////////////////////

  function setupChannelAdjustmentEditor() {

    createSection('channelAdjustment', $.i18n("remote_adjustment_label"), '', 'fa-wifi', 'remote_adjustment_intro', {}, 'card-default', '');

    const channelAdjustmentSubmitButton = document.getElementById('btn_submit_channelAdjustment');
    const channelAdjustmentDefaultsButtonId = 'btn_submit_channelAdjustmentDefaults';
    let channelAdjustmentDefaultsButton = document.getElementById(channelAdjustmentDefaultsButtonId);
    if (channelAdjustmentSubmitButton && !document.getElementById(channelAdjustmentDefaultsButtonId)) {
      const defaultsButton = document.createElement('button');
      defaultsButton.className = 'btn btn-primary me-2';
      defaultsButton.id = channelAdjustmentDefaultsButtonId;
      defaultsButton.type = 'button';
      defaultsButton.innerHTML = '<i class="fa fa-fw fa-undo"></i><span data-i18n="general_btn_reset">' + $.i18n('general_btn_reset') + '</span>';
      defaultsButton.addEventListener('click', resetToChannelAdjustmentConfig);
      channelAdjustmentSubmitButton.parentNode.insertBefore(defaultsButton, channelAdjustmentSubmitButton);
      channelAdjustmentDefaultsButton = defaultsButton;
    }

    applyResponsiveActionButtonsLayout(channelAdjustmentSubmitButton, channelAdjustmentDefaultsButton);

    let channelAdjustmenSchema = globalThis.schema.channelAdjustment;
    channelAdjustmenSchema.properties.id = { options: { hidden: true } };
    channelAdjustmenSchema.properties.leds = { options: { hidden: true } };
    const startval = {
      channelAdjustment: globalThis.serverConfig.color.channelAdjustment && globalThis.serverConfig.color.channelAdjustment.length > 0
        ? globalThis.serverConfig.color.channelAdjustment[0]
        : {}
    };

    console.log("Starting ChannelAdjustment Editor with value: " + JSON.stringify(startval));
    createEditor(editors, 'channelAdjustment', 'channelAdjustment', handleChannelAdjustmentChange, {
      bindDefaultChange: false,
      bindSubmit: true,
      onSubmit: saveChannelAdjustmentConfig,
      startval: startval
    });

    $('#btn_submit_channelAdjustment').prop('disabled', !isCurrentInstanceRunning());
    $('#btn_submit_channelAdjustmentDefaults').prop('disabled', !isCurrentInstanceRunning());
  }

  function resetToChannelAdjustmentConfig() {
    console.log("Setting Channel Adjustment config values...");
    editors['channelAdjustment'].setValue({
      channelAdjustment: globalThis.serverConfig.color.channelAdjustment && globalThis.serverConfig.color.channelAdjustment.length > 0
        ? globalThis.serverConfig.color.channelAdjustment[0]
        : {}
    });
  }

  function saveChannelAdjustmentConfig() {
    if (editors['channelAdjustment'].ready) {
      console.log("Submitting ChannelAdjustment configuration: " + JSON.stringify(editors['channelAdjustment'].getValue()));
      requestWriteConfig(editors['channelAdjustment'].getValue());
    }
  }

  // Update the channel adjustments
  function updateChannelAdjustments() {
    if (!globalThis.serverInfo.adjustment || globalThis.serverInfo.adjustment.length === 0) {
      return;
    }

    debugMessage("Updating channel adjustments with: " + JSON.stringify(globalThis.serverInfo.adjustment[0]));

    const values = globalThis.serverInfo.adjustment[0];
    if (editors['channelAdjustment']?.ready) {
      editors['channelAdjustment'].setValue({ channelAdjustment: values });
    }
  }

  /////////////////////////////////
  /// Input Source Management
  /////////////////////////////////

  function buildOwnerColorBadge(value) {
    const badgeStyle = [
      'width:18px',
      'height:18px',
      'border-radius:20px',
      'margin-bottom:-4px',
      'border:1px grey solid',
      `background-color: rgb(${value})`,
      'display:inline-block'
    ].join('; ');

    return `<div style="${badgeStyle}" title="RGB: (${value})"></div>`;
  }

  function setupSourceSelection() {
    createSection('sourceSelection', $.i18n("remote_input_label"), '', 'fa-wifi', 'remote_input_intro', {}, 'card-default', '');
    createBootstrapTable('sourceSelectionTableHead', 'sourceSelectionTableBody', 'editor_body_sourceSelection', {
      borderless: true,
      tableClasses: ['w-100'],
      columnWidths: ['25%', '40%', '10%', '25%']
    });
    $('.sourceSelectionTableHead').html(createBootstrapTableRow([
      $.i18n('remote_input_origin'),
      $.i18n('remote_input_owner'),
      $.i18n('remote_input_priority'),
      $.i18n('remote_input_status')
    ], {
      isHeader: true,
      alignMiddle: true,
      columnClasses: ['', '', 'text-nowrap', 'text-end']
    }));
    setupAutoButtons();
  }

  function resolveOwnerText(componentId, owner, value) {
    if (componentId === "EFFECT") {
      return `${$.i18n('remote_effects_label_effects')}` + (owner ? ": " + `${owner}` : '');
    }

    if (componentId === "COLOR") {
      return `${$.i18n('remote_color_label_color')} ${buildOwnerColorBadge(value)}`;
    }

    if (componentId === "IMAGE") {
      return $.i18n('remote_effects_label_picture') + (owner ? ": " + `${owner}` : '');
    }

    if (inputSourcesWithOwnerLabel.has(componentId)) {
      return `${$.i18n('general_comp_' + componentId)}: (${owner})`;
    }

    if (inputSourcesWithoutOwnerLabel.has(componentId)) {
      return $.i18n('general_comp_' + componentId);
    }

    return owner;
  }

  function resolveButtonState(active, visible) {
    if (visible) {
      return {
        btnType: "success",
        btnText: $.i18n('remote_input_sourceactiv_btn'),
        btnState: "disabled"
      };
    }

    return {
      btnType: active ? "primary" : "default",
      btnText: $.i18n('remote_input_setsource_btn'),
      btnState: "enabled"
    };
  }

  function isClearablePriority(componentId, priority) {
    return priority < BG_PRIORITY && ["EFFECT", "COLOR", "IMAGE"].includes(componentId);
  }

  function shouldShowDuration(componentId, durationSeconds) {
    return durationSeconds > 0 && !["GRABBER", "FLATBUFSERVER", "PROTOSERVER"].includes(componentId);
  }

  function buildPriorityOriginText(priorityEntry) {
    const { originName, ip } = parsePriorityOrigin(priorityEntry);
    if (!ip) {
      return originName;
    }

    return `${originName}<br/><span style="font-size:80%; color:grey;">${$.i18n('remote_input_ip')} ${ip}</span>`;
  }

  function appendPriorityDuration(ownerText, durationSeconds) {
    return `${ownerText}<br/><span style="font-size:80%; color:grey;">${$.i18n('remote_input_duration')} ${durationSeconds.toFixed(0)}${$.i18n('edt_append_s')}</span>`;
  }

  function appendNoSourcesRow() {
    $('.sourceSelectionTableBody').append(`<tr><td colspan="4" class="text-center text-muted">${$.i18n('remote_input_no_sources')}</td></tr>`);
  }

  function appendSourceSelectionRow(origin, ownerText, priority, btn) {
    const row = createBootstrapTableRow([origin, ownerText, priority, btn], {
      alignMiddle: true,
      columnClasses: ['', '', 'text-nowrap', 'text-end']
    });

    $('.sourceSelectionTableBody').append(row);
  }

  function buildSourceButtonHtml(index, priority, btnState, btnType, btnText, componentId) {
    let btn = `<button id="srcBtn${index}" type="button" ${btnState} class="btn btn-${btnType} btn_input_selection" onclick="requestSetSource(${priority});">${btnText}</button>`;

    if (isClearablePriority(componentId, priority)) { 
      btn += `<button type="button" class="btn btn-sm btn-danger" style="margin-left:10px;" onclick="requestPriorityClear(${priority});"><i class="fa fa-close"></i></button>`;
    }

    return btn;
  }

  function setupAutoButtons() {
    const sourceSelectionSubmitButton = document.getElementById('btn_submit_sourceSelection');

    if (!sourceSelectionSubmitButton?.parentElement) {
      return;
    }

    const sourceSelectionActionsContainer = sourceSelectionSubmitButton.parentElement;
    sourceSelectionActionsContainer.replaceChildren();
    sourceSelectionActionsContainer.style.textAlign = 'left';
    sourceSelectionActionsContainer.classList.add('d-flex', 'align-items-center', 'justify-content-between', 'w-100');

    const autoSwitchContainer = document.createElement('div');
    autoSwitchContainer.className = 'form-check form-switch form-switch-md d-inline-flex align-items-center gap-2 mb-0';

    const autoSwitchInput = document.createElement('input');
    autoSwitchInput.className = 'form-check-input';
    autoSwitchInput.setAttribute('role', 'switch');
    autoSwitchInput.id = 'srcSwitchAuto';
    autoSwitchInput.type = 'checkbox';
    autoSwitchInput.setAttribute('switch', '');
    autoSwitchInput.addEventListener('change', () => {
      requestSetSource('auto');
    });

    const autoSwitchLabel = document.createElement('label');
    autoSwitchLabel.className = 'form-check-label';
    autoSwitchLabel.setAttribute('for', 'srcSwitchAuto');
    autoSwitchLabel.id = 'srcSwitchAutoLabel';

    autoSwitchContainer.appendChild(autoSwitchInput);
    autoSwitchContainer.appendChild(autoSwitchLabel);

    const clearAllButton = document.createElement('button');
    clearAllButton.id = 'srcBtnClearAll';
    clearAllButton.type = 'button';
    clearAllButton.className = 'btn btn-primary';
    clearAllButton.textContent = $.i18n('remote_input_clearall');
    clearAllButton.addEventListener('click', () => {
      requestClearAll();
    });

    sourceSelectionActionsContainer.appendChild(autoSwitchContainer);
    sourceSelectionActionsContainer.appendChild(clearAllButton);
  }

  function updateAutoButtons(clearAll) {
    const autoText = globalThis.serverInfo.priorities_autoselect ? $.i18n('general_btn_on') : $.i18n('general_btn_off');
    const isRunning = isCurrentInstanceRunning();
    const autoSwitchInput = document.getElementById('srcSwitchAuto');
    const autoSwitchLabel = document.getElementById('srcSwitchAutoLabel');
    const clearAllButton = document.getElementById('srcBtnClearAll');

    if (!autoSwitchInput || !autoSwitchLabel || !clearAllButton) {
      setupAutoButtons();
    }

    const ensuredAutoSwitchInput = document.getElementById('srcSwitchAuto');
    const ensuredAutoSwitchLabel = document.getElementById('srcSwitchAutoLabel');
    const ensuredClearAllButton = document.getElementById('srcBtnClearAll');

    if (!ensuredAutoSwitchInput || !ensuredAutoSwitchLabel || !ensuredClearAllButton) {
      return;
    }

    ensuredAutoSwitchInput.checked = globalThis.serverInfo.priorities_autoselect;
    ensuredAutoSwitchInput.disabled = !isRunning || globalThis.serverInfo.priorities_autoselect;
    ensuredAutoSwitchLabel.textContent = `${$.i18n('remote_input_label_autoselect')} (${autoText})`;
    ensuredClearAllButton.disabled = !isRunning || !clearAll;
  }
  
  // Update input select options based on priorities
  function updateInputSelect() {
    // Clear existing elements
    $('.sourceSelectionTableBody').empty().html('');
    debugMessage("Updating input select with priorities: " + JSON.stringify(globalThis.serverInfo.priorities));

    const prios = globalThis.serverInfo.priorities;
    let clearAll = false;
    let renderedRows = 0;

    if (prios.length === 0) {
      appendNoSourcesRow();
      updateAutoButtons(false);
      return;
    }

    // Iterate over priorities
    prios.forEach((priorityEntry, index) => {
      const { owner, active, visible, priority, componentId, duration_ms } = priorityEntry;

      if (priority > BG_PRIORITY) {
        return;
      }

      if (isClearablePriority(componentId, priority)) {
        clearAll = true;
      }

      const remoteInputDuration_s = Number.isFinite(duration_ms) ? duration_ms / 1000 : 0;
      const origin = buildPriorityOriginText(priorityEntry);

      const value = "value" in priorityEntry ? priorityEntry.value.RGB : "0,0,0";
      let ownerText = resolveOwnerText(componentId, owner, value);

      if (shouldShowDuration(componentId, remoteInputDuration_s)) {
        ownerText = appendPriorityDuration(ownerText, remoteInputDuration_s);
      }

      if (remoteInputDuration_s > 0 || remoteInputDuration_s === 0) {
        const { btnType, btnText, btnState } = resolveButtonState(active, visible);
        if (btnType !== 'default') {
          const btn = buildSourceButtonHtml(index, priority, btnState, btnType, btnText, componentId);
          appendSourceSelectionRow(origin, ownerText, priority, btn);
          renderedRows += 1;
        }
      }
    });

    if (renderedRows === 0) {
      appendNoSourcesRow();
      clearAll = false;
    }

    updateAutoButtons(clearAll);
  }

  /////////////////////////////////
  /// Component Management
  /////////////////////////////////

  function shouldSkipComponent(componentName) {
    // Define conditions to skip certain components
    const skipConditions = {
      "ALL": false,
      "FORWARDER": globalThis.currentHyperionInstance !== globalThis.serverConfig.forwarder.instance,
      "GRABBER": !globalThis.serverConfig.framegrabber.enable,
      "V4L": !globalThis.serverConfig.grabberV4L2.enable,
      "AUDIO": !globalThis.serverConfig.grabberAudio.enable
    };

    return skipConditions[componentName] || componentName === "ALL";
  }

  function createSwitchHtml(id, isChecked, isDisabled, componentName = null) {
    const wrapper = document.createElement('div');
    wrapper.className = 'form-check form-switch form-switch-md d-inline-flex align-items-center m-0 ps-0';
    wrapper.style.minHeight = '1.5rem';

    const input = document.createElement('input');
    input.className = 'form-check-input m-0 align-self-center';
    input.style.marginLeft = '0';
    input.type = 'checkbox';
    input.role = 'switch';
    input.id = id;
    input.defaultChecked = isChecked;
    input.disabled = isDisabled;
    if (componentName) {
      input.dataset.name = componentName;
    }
    input.setAttribute('switch', '');

    wrapper.appendChild(input);
    return wrapper.outerHTML;
  }

  function createComponentRow(componentName, isEnabled) {
    const tr = document.createElement('tr');
    const switchId = `comp_btn_${componentName}`;

    const tdComponent = document.createElement('td');
    tdComponent.colSpan = 2;
    tdComponent.style.verticalAlign = 'middle';
    tdComponent.className = 'text-start py-2';

    const wrapper = document.createElement('div');
    wrapper.className = 'd-flex align-items-center justify-content-start gap-4';

    const switchWrap = document.createElement('div');
    switchWrap.className = 'd-flex align-items-center flex-shrink-0';
    switchWrap.innerHTML = createSwitchHtml(switchId, isEnabled, false, componentName);

    const label = document.createElement('label');
    label.className = 'mb-0 d-flex align-items-center ms-1';
    label.setAttribute('for', switchId);
    label.textContent = $.i18n(`general_comp_${componentName}`);

    wrapper.appendChild(switchWrap);
    wrapper.appendChild(label);
    tdComponent.appendChild(wrapper);
    tr.appendChild(tdComponent);

    return tr.outerHTML;
  }

  function initComponents() {
    const components = globalThis.comps;

    const isInstanceEnabled = components.some((comp) => comp.name === "ALL" && comp.enabled);
    const componentRows = components
      .filter((element) => !shouldSkipComponent(element.name))
      .map((element) => createComponentRow(element.name, isInstanceEnabled ? element.enabled : false))
      .join('');
    $('#components').html(componentRows);

    components
      .filter((comp) => !shouldSkipComponent(comp.name))
      .forEach((comp) => {
        const $switch = getComponentSwitch(comp.name);
        if ($switch.length === 0) {
          return;
        }

        setComponentSwitchState(comp.name, { disabled: !(isInstanceEnabled && isCurrentInstanceRunning()) });
        $switch.off('change').on('change', (e) => {
          requestSetComponentState(comp.name, e.currentTarget.checked);
        });
      });
  }

  function getComponentSwitchId(componentName) {
    return `comp_btn_${componentName}`;
  }

  function getComponentSwitch(componentName) {
    return $(`#${getComponentSwitchId(componentName)}`);
  }

  function setComponentSwitchState(componentName, { checked, disabled } = {}) {
    const $switch = getComponentSwitch(componentName);
    if ($switch.length === 0) {
      return $switch;
    }

    if (checked !== undefined && $switch.prop('checked') !== checked) {
      $switch.prop('checked', checked);
    }

    if (disabled !== undefined) {
      $switch.prop('disabled', disabled);
    }

    return $switch;
  }

  function updateComponent(component) {
    if (component.name === "ALL") {
      updateAllComponents(component.enabled);
    } else {
      updateSingleComponent(component);
    }
  }

  function updateAllComponents(enabled) {
    globalThis.comps.forEach((comp) => {
      if (comp.name === "ALL") return;

      if (enabled) {
        setComponentSwitchState(comp.name, { disabled: false });
        updateSingleComponent(comp);
      } else {
        setComponentSwitchState(comp.name, { checked: false, disabled: true });
      }
    });
  }

  function updateSingleComponent(component) {
    setComponentSwitchState(component.name, { checked: component.enabled });
  }



  // Update Video Mode
  function updateVideoMode() {
    const videoModes = ["3DSBS", "3DTAB", "2D"];
    const currVideoMode = globalThis.serverInfo.videomode;
    const isRunning = isCurrentInstanceRunning();
    const videoModeContainer = document.getElementById('videomodebtns');

    if (!videoModeContainer) {
      return;
    }

    videoModeContainer.replaceChildren();
    videoModeContainer.classList.add('d-flex', 'flex-wrap', 'gap-2');

    videoModes.forEach((mode) => {
      const btnStyle = currVideoMode === mode ? 'btn-success' : 'btn-primary';
      const button = document.createElement('button');
      button.type = 'button';
      button.id = `vModeBtn_${mode}`;
      button.className = `btn ${btnStyle}`;
      button.style.margin = '0';
      button.style.minWidth = '140px';
      button.style.flex = '1 1 140px';
      button.disabled = !isRunning;
      button.textContent = $.i18n('remote_videoMode_' + mode);
      button.addEventListener('click', () => {
        requestVideoMode(mode);
      });

      videoModeContainer.appendChild(button);
    });
  }

  // Interval Updates and Event Handlers
  function setupEventListenersForUpdates() {
    $(globalThis.hyperion).on('components-updated', (e, comp) => updateComponent(comp));

    $(globalThis.hyperion).on("cmd-priorities-update", (event) => {
      globalThis.serverInfo.priorities = event.response.data.priorities;
      globalThis.serverInfo.priorities_autoselect = event.response.data.priorities_autoselect;
      updateInputSelect();
    });

    $(globalThis.hyperion).on("cmd-imageToLedMapping-update", (event) => {
      globalThis.serverInfo.imageToLedMappingType = event.response.data.imageToLedMappingType;
      updateLedMapping();
    });

    $(globalThis.hyperion).on("cmd-videomode-update", (event) => {
      globalThis.serverInfo.videomode = event.response.data.videomode;
      updateVideoMode();
    });

    $(globalThis.hyperion).on("cmd-effects-update", (event) => {
      globalThis.serverInfo.effects = event.response.data.effects;
      updateColorEffectsEditor();
    });

    $(globalThis.hyperion).on("cmd-settings-update", (event) => {
      if (event.response.data.color) {
        globalThis.serverInfo.imageToLedMappingType = event.response.data.color.imageToLedMappingType;
        updateLedMapping();
        globalThis.serverInfo.adjustment = event.response.data.color.channelAdjustment;
        updateChannelAdjustments();
      }
    });


  }
});

function handleChannelAdjustmentChange(channelEditor) {

  const updateChannelAdjustmentButtons = () => {
    const isRunning = isCurrentInstanceRunning();
    const isValid = channelEditor.validate().length === 0 && !globalThis.readOnlyMode;

    $('#btn_submit_channelAdjustment').prop('disabled', !(isRunning && isValid));
    $('#btn_submit_channelAdjustmentDefaults').prop('disabled', !isRunning);
  };

  channelEditor.on('ready', () => {
    updateChannelAdjustmentButtons();

    if (!isCurrentInstanceRunning()) {
      channelEditor.disable();
      return;
    }

    const allEditors = Object.values(channelEditor.editors);

    for (const editor of allEditors) {

      if (!editor?.input) {
        continue;
      }

      if (editor.input_type !== 'color' && editor.input.type !== 'color') {
        continue;
      }

      const input = editor.input;

      // add Bootstrap styling
      //input.classList.add('form-control', 'form-control-color');
      input.dataset.editorKey = editor.key;
      input.dataset.editorPath = editor.path;

      const sendColorAdjustment = (adjustmentKey, color) => {
        const rgb = hexToRgb(color);
        if (!rgb) {
          return;
        }

        debugMessage('Color adjustment event. Color: ' + adjustmentKey + ' -> RGB: ' + JSON.stringify(rgb));
        requestAdjustment(adjustmentKey, [rgb.r, rgb.g, rgb.b]);
      };

      const sendColorAdjustmentDebounced = debounce(sendColorAdjustment, 150);

      input.addEventListener('input', (e) => {
        const { editorKey } = input.dataset;
        sendColorAdjustmentDebounced(editorKey, e.currentTarget.value);
      });

      input.addEventListener('change', () => {
        const { editorKey, editorPath } = input.dataset;
        const currentEditor = channelEditor.editors[editorPath];
        if (!currentEditor) {
          return;
        }

        const currentValue = currentEditor.getValue();
        sendColorAdjustmentDebounced.cancel();
        sendColorAdjustment(editorKey, currentValue);
      });
    }
  });

  channelEditor.on('change', () => {
    updateChannelAdjustmentButtons();
  });
}

function handleImageProcessingChange(editor) {

  const updateImageProcessingButtons = () => {
    const isRunning = isCurrentInstanceRunning();
    const isValid = editor.validate().length === 0 && !globalThis.readOnlyMode;

    $('#btn_submit_imageProcessing').prop('disabled', !(isRunning && isValid));
    $('#btn_submit_imageProcessingDefaults').prop('disabled', !isRunning);
  };

  editor.on('ready', () => {
    updateImageProcessingButtons();

    if (!isCurrentInstanceRunning()) {
      editor.disable();
    }
  });

  editor.on('change', () => {
    updateImageProcessingButtons();
  });

  editor.watch('root.imageProcessing.imageToLedMappingType', () => {
    if (!editor.ready) return;
    const mappingType = editor.getEditor("root.imageProcessing.imageToLedMappingType").getValue();

    debugMessage("Image to LED mapping type changed to: " + mappingType);
  });

  editor.watch('root.imageProcessing.accuracyLevel', () => {
    if (!editor.ready) return;
    const accuracyLevel = editor.getEditor("root.imageProcessing.accuracyLevel").getValue();

    debugMessage("Accuracy level changed to: " + accuracyLevel);
  });

  editor.watch('root.imageProcessing.reducedPixelSetFactorFactor', () => {
    if (!editor.ready) return;
    const reducedPixelSetFactor = editor.getEditor("root.imageProcessing.reducedPixelSetFactorFactor").getValue();

    debugMessage("Reduced pixel set factor changed to: " + reducedPixelSetFactor);
  });

}


