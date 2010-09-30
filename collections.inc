<?php
// $Id$

/**
 * @file
 * Field collections editing UI.
 */

/**
 * Displays the field collections admin overview page.
 */
function field_collections_overview() {
  $list = field_collections_get_list();
  $names = field_collections_get_names();
  $header = array(t('Name'), array('data' => t('Operations'), 'colspan' => '4'));
  $rows = array();

  foreach ($names as $name => $title) {
//    dpm($names);
    $collection = $list[$name];

    $url_str = str_replace('_', '-', $collection->name);
    $row = array(theme('field_collection_admin_overview', array('title' => $title, 'collection' => $collection)));
    // Set the edit column.
    $row[] = array('data' => l(t('edit'), 'admin/structure/field-collections/manage/' . $url_str));

    // Manage fields.
    $row[] = array('data' => l(t('manage fields'), 'admin/structure/field-collections/manage/' . $url_str . '/fields'));

    // Display fields.
    $row[] = array('data' => l(t('manage display'), 'admin/structure/field-collections/manage/' . $url_str . '/display'));

    // Set the delete column.
    $row[] = array('data' => l(t('delete'), 'admin/structure/field-collections/manage/' . $url_str . '/delete'));

    $rows[] = $row;
  }

  $build['collections_table'] = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#empty' => t('No collections available. <a href="@link">Add collection</a>.', array('@link' => url('admin/structure/field-collections/add'))),
  );

  return $build;
}

/**
 * Generates the field collection editing form.
 */
function field_collection_form($form, &$form_state, $collection = NULL) {
  if (! isset($collection->name)) {
    // This is a new type, set defaults.
    $collection = field_collection_set_defaults();
  }

  // Make the collection object available to implementations of hook_form_alter.
  $form['#collection'] = $collection;

  $form['title'] = array(
    '#title' => t('Title'),
    '#type' => 'textfield',
    '#default_value' => $collection->title,
    '#description' => t('The human-readable name of this field collection. It is recommended that this title begin with a capital letter and contain only letters, numbers, and spaces. This title must be unique.'),
    '#required' => TRUE,
    '#size' => 30,
    '#field_suffix' => ' <small id="edit-title-suffix">' . t('Machine name: @name', array('@name' => $collection->name)) . '</small>',
  );

  $js_settings = array(
    'type' => 'setting',
    'data' => array(
      'machineReadableValue' => array(
        'title' => array(
          'text' => t('Machine name'),
          'target' => 'name',
          'searchPattern' => '[^a-z0-9]+',
          'replaceToken' => '_',
        ),
      ),
    ),
  );
  $form['name'] = array(
    '#title' => t('Machine name'),
    '#type' => 'textfield',
    '#default_value' => $collection->name,
    '#maxlength' => 32,
    '#required' => TRUE,
    '#description' => t('The machine-readable name of this field collection. This name must contain only lowercase letters, numbers, and underscores. Underscores will be converted into hyphens when constructing the URL. This name must be unique.'),
    '#attached' => array(
      'js' => array(drupal_get_path('module', 'system') . '/system.js', $js_settings),
    ),
  );

  $form['description'] = array(
    '#title' => t('Description'),
    '#type' => 'textarea',
    '#default_value' => $collection->description,
    '#description' => t('Describe this field collection.'),
  );

  $form['old_name'] = array(
    '#type' => 'value',
    '#value' => $collection->name,
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save field collection'),
    '#weight' => 40,
  );

  if (!empty($collection->name)) {
    $form['actions']['delete'] = array(
      '#type' => 'submit',
      '#value' => t('Delete field collection'),
      '#weight' => 45,
    );
  }

  return $form;
}

/**
 * Implements hook_form_submit() for field_collection_form().
 */
function field_collection_form_submit($form, &$form_state) {
  $op = isset($form_state['values']['op']) ? $form_state['values']['op'] : '';

  $collection = field_collection_set_defaults();

  $collection->title = trim($form_state['values']['title']);
  $collection->name = trim($form_state['values']['name']);
  $collection->old_name = isset($form_state['values']['old_name']) ? $form_state['values']['old_name'] : $collection->name;

  $collection->description = $form_state['values']['description'];

  if ($op == t('Delete field collection')) {
    $form_state['redirect'] = 'admin/structure/field-collections/manage/' . str_replace('_', '-', $collection->old_name) . '/delete';
    return;
  }

  $status = field_collection_save($collection);

  field_collections_rebuild();
  menu_rebuild();
  $t_args = array('%title' => $collection->title);

  if ($status == SAVED_UPDATED) {
    drupal_set_message(t('The field collection %title has been updated.', $t_args));
  }
  elseif ($status == SAVED_NEW) {
    drupal_set_message(t('The field collection %title has been added.', $t_args));
    watchdog('node', 'Added field collection %title.', $t_args, WATCHDOG_NOTICE, l(t('view'), 'admin/structure/field-collections'));
  }

  $form_state['redirect'] = 'admin/structure/field-collections';
  return;
}

/**
 * Validates the field collection type submission form generated by field_collection_type_form().
 */
function field_collection_type_form_validate($form, &$form_state) {
  $collection = new stdClass();
  $collection->title = trim($form_state['values']['title']);
  $collection->name = trim($form_state['values']['name']);

  // Work out what the type was before the user submitted this form
  $old_name = trim($form_state['values']['old_name']);

  $names = field_collections_get_names();

  if (isset($names[$collection->name]) && $collection->name != $old_name) {
    form_set_error('name', t('The machine-readable name %name is already taken.', array('%name' => $collection->name)));
  }
  if (!preg_match('!^[a-z0-9_]+$!', $collection->name)) {
    form_set_error('name', t('The machine-readable name must contain only lowercase letters, numbers, and underscores.'));
  }
  // 'theme' conflicts with theme_node_form().
  // '0' is invalid, since elsewhere we check it using empty().
  if (in_array($collection->name, array('0', 'theme'))) {
    form_set_error('name', t("Invalid machine-readable name. Enter a name other than %invalid.", array('%invalid' => $collection->name)));
  }

  $titles = array_flip($names);

  if (isset($titles[$collection->title]) && $titles[$collection->title] != $old_name) {
    form_set_error('name', t('The human-readable title %title is already taken.', array('%title' => $collection->title)));
  }
}

/**
 * Returns HTML for a collection description for the collection admin overview page.
 *
 * @param $variables
 *   An associative array containing:
 *   - title: The human-readable name of the collection.
 *   - collection: An object containing the 'name' (machine name) and
 *      'description' of the collection.
 */
function theme_field_collection_admin_overview($variables) {
  $title = $variables['title'];
  $collection = $variables['collection'];

  $output = check_plain($title);
  $output .= ' <small> (Machine name: ' . check_plain($collection->name) . ')</small>';
  $output .= '<div class="description">' . filter_xss_admin($collection->description) . '</div>';
  return $output;
}
