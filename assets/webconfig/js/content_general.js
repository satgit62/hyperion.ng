$(document).ready(function () {
  // Perform translation on page load
  performTranslation();

  let importedConf;
  let confName;
  const editors = {};

  initializeUI();
  setupEditors();
  buildInstanceList();
  removeOverlay();

  function initializeUI() {

    createSystemSection("general", "edt_conf_gen_heading_title", globalThis.schema.general.properties, "fa-wrench", "conf_general_intro", "generalHelpPanelId");

    createSection("instance", "conf_general_inst_title", '', "mdi mdi-lightbulb-group", "conf_general_inst_desc", false, 'card-instance');
    createBootstrapTable('instanceTableHead', 'instanceTableBody', 'editor_body_instance', {
      borderless: false,
      tableClasses: ['w-100'],
      columnWidths: ['70%', '30%']
    });
    $('.instanceTableHead').html(createBootstrapTableRow([
      $.i18n('conf_general_inst_namehead'),
      $.i18n('conf_general_inst_actionhead')
    ], {
      isHeader: true,
      alignMiddle: true,
      columnClasses: ['text-nowrap', 'text-center']
    }));

    const $instanceFooterButton = $('#btn_submit_instance');
    const $instanceFooter = $instanceFooterButton.closest('.card-footer');
    $instanceFooter.empty();

    createSystemSection("import", "conf_general_impexp_title", '', "fa-wrench", "conf_general_impexp_l1", false);
    const $importBody = $('#editor_body_import');
    const $importFooterButton = $('#btn_submit_import');
    const $importFooter = $importFooterButton.closest('.card-footer');
    const $importDescription = $('<div>', { id: 'imp_desc_cont' });
    const $importFileInput = $('<input>', {
      class: 'form-control',
      type: 'file',
      id: 'select_import_conf',
      accept: '.json'
    });
    const $importFooterActions = $('<div>', {
      class: 'd-flex justify-content-end align-items-center gap-2'
    });
    const $importButton = $('<button>', {
      type: 'button',
      class: 'btn btn-primary',
      id: 'btn_import_conf',
      html: `<i class="fa fa-fw fa-upload"></i>${$.i18n('conf_general_impexp_impbtn')}`
    }).prop('disabled', true);
    const $exportButton = $('<button>', {
      type: 'button',
      class: 'btn btn-primary',
      id: 'btn_export_conf',
      html: `<i class="fa fa-fw fa-download"></i>${$.i18n('conf_general_impexp_expbtn')}`
    });

    $importBody.append($importDescription, $importFileInput);
    $importFooterActions.append($importButton, $exportButton);
    $importFooter.empty().append($importFooterActions);
  }

  function setupEditors() {
    createEditor(editors, 'general', 'general', '', {
      bindDefaultChange: true,
      bindSubmit: true,
      onSubmit: function () {
        globalThis.showOptHelp = editors["general"].getEditor("root.general.showOptHelp").getValue();
        requestWriteConfig(editors["general"].getValue());
      }

    });
  }

  // Instance handling functions
  function handleInstanceRename(instance) {
    showInfoDialog('renInst', $.i18n('conf_general_inst_renreq_t'), getInstanceName(instance));

    // Rename button click handler
    $("#id_btn_ok").off().on('click', function () {
      requestInstanceRename(instance, encodeHTML($('#renInst_name').val()));
    });

    // Input handler for rename field
    $('#renInst_name').off().on('input', function (e) {
      const isValid = e.currentTarget.value.length >= 5 && e.currentTarget.value !== getInstanceName(instance);
      $('#id_btn_ok').prop('disabled', !isValid);
    });
  }

  function handleInstanceDelete(instance) {
    showInfoDialog('delInst', $.i18n('conf_general_inst_delreq_h'), $.i18n('conf_general_inst_delreq_t', getInstanceName(instance)));

    // Delete button click handler
    $("#id_btn_yes").off().on('click', function () {
      requestInstanceDelete(instance);
    });
  }

  // Build the instance list
  function buildInstanceList() {

    const $instanceTableBody = $('.instanceTableBody');
    if ($instanceTableBody.length === 0) {
      console.warn("Element '.instanceTableBody' does not exist. Aborting instance list build.");
      return;
    }

    const data = globalThis.serverInfo.instance;
    if (data) {
      const instances = Object.values(data);

      // Sort instances by friendly_name (case-insensitive)
      instances.sort((a, b) => a.friendly_name.toLowerCase().localeCompare(b.friendly_name.toLowerCase()));

      $instanceTableBody.empty(); // Explicitly clear the content before adding new rows

      // Collect rows in a document fragment for efficient DOM updates
      const $rows = $(document.createDocumentFragment());

      // Build all instance rows
      for (const instance of instances) {
        const instanceID = instance.instance;

        const startBtn = document.createElement('div');
        startBtn.className = 'form-check form-switch form-switch-md m-0 d-flex align-items-center';
        const startInput = document.createElement('input');
        startInput.className = 'form-check-input mt-0';
        startInput.type = 'checkbox';
        startInput.setAttribute('role', 'switch');
        startInput.id = `inst_${instanceID}`;
        startInput.setAttribute('switch', '');
        startInput.checked = instance.running;
        startBtn.appendChild(startInput);

        const renameBtn = document.createElement('button');
        renameBtn.type = 'button';
        renameBtn.className = 'btn btn-outline-secondary d-inline-flex align-items-center justify-content-center';
        renameBtn.id = `instren_${instanceID}`;
        renameBtn.style.width = '2.8rem';
        renameBtn.style.height = '2.5rem';
        const renameIcon = document.createElement('i');
        renameIcon.className = 'mdi mdi-lead-pencil fs-5';
        renameBtn.appendChild(renameIcon);

        const delBtn = document.createElement('button');
        delBtn.type = 'button';
        delBtn.className = 'btn btn-outline-danger d-inline-flex align-items-center justify-content-center';
        delBtn.id = `instdel_${instanceID}`;
        delBtn.style.width = '2.8rem';
        delBtn.style.height = '2.5rem';
        const delIcon = document.createElement('i');
        delIcon.className = 'mdi mdi-playlist-remove fs-5';
        delBtn.appendChild(delIcon);

        const actionContainer = document.createElement('div');
        actionContainer.className = 'd-flex justify-content-end align-items-center gap-3';
        actionContainer.append(startBtn, renameBtn, delBtn);

        const $row = createTableRow(
          [instance.friendly_name, actionContainer],
          false,
          true
        );

        $rows.append($row);
      }


      const createInput = document.createElement('input');
      createInput.type = 'text';
      createInput.id = 'instance_name';
      createInput.className = 'form-control';
      createInput.placeholder = $.i18n('conf_general_inst_name_title');
      createInput.setAttribute('aria-describedby', 'instance_chars_needed');

      const createBtn = document.createElement('button');
      createBtn.type = 'button';
      createBtn.className = 'btn btn-outline-primary d-inline-flex align-items-center justify-content-center';
      createBtn.id = 'btn_create_inst';
      createBtn.disabled = true;
      createBtn.title = $.i18n('conf_general_createInst_btn');
      createBtn.style.width = '2.8rem';
      createBtn.style.height = '2.5rem';
      const createIcon = document.createElement('i');
      createIcon.className = 'mdi mdi-playlist-plus fs-5';
      createBtn.appendChild(createIcon);

      const switchSpacer = document.createElement('div');
      switchSpacer.className = 'form-check form-switch form-switch-md m-0 d-flex align-items-center invisible';
      const switchSpacerInput = document.createElement('input');
      switchSpacerInput.className = 'form-check-input mt-0';
      switchSpacerInput.type = 'checkbox';
      switchSpacer.appendChild(switchSpacerInput);

      const deleteSpacer = document.createElement('button');
      deleteSpacer.type = 'button';
      deleteSpacer.className = 'btn btn-outline-danger d-inline-flex align-items-center justify-content-center invisible';
      deleteSpacer.style.width = '2.8rem';
      deleteSpacer.style.height = '2.5rem';
      const deleteSpacerIcon = document.createElement('i');
      deleteSpacerIcon.className = 'mdi mdi-delete-forever fs-5';
      deleteSpacer.appendChild(deleteSpacerIcon);

      const createActionContainer = document.createElement('div');
      createActionContainer.className = 'd-flex justify-content-end align-items-center gap-3';
      createActionContainer.append(switchSpacer, deleteSpacer, createBtn);

      const $createRow = createTableRow(
        [createInput, createActionContainer],
        false,
        true
      );

      $rows.append($createRow);

      const createHint = document.createElement('span');
      createHint.id = 'instance_chars_needed';
      createHint.className = 'form-text';
      createHint.innerHTML = '&nbsp;';

      const $createHintRow = createTableRow(
        [createHint, document.createElement('div')],
        false,
        true
      );

      $rows.append($createHintRow);

      $instanceTableBody.append($rows);

      // Apply Bootstrap toggles and event handlers
      for (const instance of instances) {
        const instanceID = instance.instance;
        const readOnly = globalThis.readOnlyMode;

        $('#instren_' + instanceID).prop('disabled', readOnly).off().on('click', function () {
          handleInstanceRename(instanceID);
        });

        $('#instdel_' + instanceID).prop('disabled', readOnly).off().on('click', function () {
          handleInstanceDelete(instanceID);
        });

        const $toggle = $('#inst_' + instanceID);
        $toggle.prop('disabled', readOnly);
        $toggle.off('change').on('change', function () {
          const isChecked = $(this).prop('checked');
          requestInstanceStartStop(instanceID, isChecked);
        });
      }
    }
  }

  // Instance name input validation
  $(document).off('input', '#instance_name').on('input', '#instance_name', function (e) {
    const isValid = e.currentTarget.value.length >= 5 && !globalThis.readOnlyMode;
    $('#btn_create_inst').prop('disabled', !isValid);

    const charsNeeded = 5 - e.currentTarget.value.length;
    const $charsHint = $('#instance_chars_needed');
    if ($charsHint.length) {
      $charsHint.html(charsNeeded >= 1 && charsNeeded <= 4 ? `${charsNeeded} ${$.i18n('general_chars_needed')}` : "<br />");
    }
  });

  // Instance creation button click handler
  $(document).off('click', '#btn_create_inst').on('click', '#btn_create_inst', function () {
    requestInstanceCreate(encodeHTML($('#instance_name').val()));
    $('#instance_name').val("");
    $('#btn_create_inst').prop('disabled', true);
  });

  // Instance updated event listener
  $(hyperion).off("instance-updated").on("instance-updated", function (event) {
    buildInstanceList();
  });

  // Import handling functions
  function dis_imp_btn(state) {
    $('#btn_import_conf').prop('disabled', state || globalThis.readOnlyMode);
  }

  async function readFile(evt) {
    const f = evt.target.files[0];
    if (f) {
      try {
        let content = await f.text();
        content = content.replaceAll(/[^:]?\/\/.*/g, ''); // Remove comments

        // Check if the content is valid JSON
        const check = isJsonString(content);
        if (check.length === 0) {
          content = JSON.parse(content);
          if (content.global === undefined || content.instances === undefined) {
            showInfoDialog('error', "", $.i18n('infoDialog_import_version_error_text', f.name));
            dis_imp_btn(true);
          } else {
            dis_imp_btn(false);
            importedConf = content;
            confName = f.name;
          }
        } else {
          showInfoDialog('error', "", $.i18n('infoDialog_import_jsonerror_text', f.name, JSON.stringify(check.message)));
          dis_imp_btn(true);
        }
      } catch (error) {
        console.error("Error reading file:", error);
        showInfoDialog('error', "", $.i18n('infoDialog_import_comperror_text'));
        dis_imp_btn(true);
      }
    }
  }

  // Import button click handler
  $('#btn_import_conf').off().on('click', function () {
    showInfoDialog('import', $.i18n('infoDialog_import_confirm_title'), $.i18n('infoDialog_import_confirm_text', confName));

    $('#id_btn_import').off().on('click', function () {
      requestRestoreConfig(importedConf);
    });
  });

  // Import file selection change handler
  $('#select_import_conf').off().on('change', function (e) {
    if (globalThis.File && globalThis.FileReader && globalThis.FileList && globalThis.Blob) {
      readFile(e);
    } else {
      showInfoDialog('error', "", $.i18n('infoDialog_import_comperror_text'));
    }
  });

  // Export configuration
  $('#btn_export_conf').off().on('click', async () => {
    const d = new Date();
    const month = String(d.getMonth() + 1).padStart(2, '0');
    const day = String(d.getDate()).padStart(2, '0');
    const timestamp = `${d.getFullYear()}-${month}-${day}`;

    const configBackup = await requestServerConfig.async();
    if (configBackup.success) {
      download(JSON.stringify(configBackup.info, null, "\t"), `HyperionBackup-${timestamp}_v${globalThis.currentVersion}.json`, "application/json");
    }
  });

});

// Command for restoring config
$(globalThis.hyperion).on("cmd-config-restoreconfig", function () {
  setTimeout(initRestart, 100);
});

