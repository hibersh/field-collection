<?php
// $Id$

function _field_collection_field_form_reset_validated(&$elements, $validated = FALSE) {
  $elements['#validated'] = $validated;
  foreach (element_children($elements) as $key) {
    _field_collection_field_form_reset_validated($elements[$key], $validated);
  }
}

function _field_collection_field_form_element_replace_after_build(&$elements, $language) {
  foreach (element_children($elements) as $key) {
    $field = isset($elements[$key][$language]['#field_name']) ? $elements[$key][$language] : NULL;
    $callbacks = isset($field['#after_build']) ? $field['#after_build'] : array();

    foreach ($callbacks as $delta => $function) {
      if ($function == 'field_form_element_after_build') {
        $callbacks[$delta] = 'field_collection_field_form_element_after_build';
        $elements[$key][$language]['#after_build'] = $callbacks;

        break;
      }
    }
  }
}

function field_collection_field_add_more_submit($form, &$form_state) {
  $cfn = $form_state['clicked_button']['#collection_field_name'];
  $delta = $form_state['clicked_button']['#collection_delta'];
  $field_name = $form_state['clicked_button']['#field_name'];
  $lang = $form_state['clicked_button']['#language'];

  if ($form_state['values'][$cfn . '_' . $delta . '_' . $field_name . '_add_more']) {
    // @todo Wow... is there any way to deal with this crazy nesting?!
    $form_state['field_collection'][$cfn][$lang][$delta]['field_item_count'][$field_name] = count($form_state['values'][$cfn][$lang][$delta][$field_name][$lang]);
  }
  $form_state['rebuild'] = TRUE;
}

function field_collection_field_add_more_js($form, $form_state) {
  // Retrieve field information.
  $cfn = $form_state['clicked_button']['#collection_field_name'];
  $delta = $form_state['clicked_button']['#collection_delta'];
  $field_name = $form_state['clicked_button']['#field_name'];
  $langcode = $form_state['clicked_button']['#language'];
  $field_info = $form_state['field_collection'][$cfn][$langcode][$delta]['field'][$field_name][$langcode];
  $field = $field_info['field'];

  if ($field['cardinality'] != FIELD_CARDINALITY_UNLIMITED) {
    return;
  }

  // Navigate to the right element in the the form.
  $element = $form;

  foreach ($field_info['array_parents'] as $key) {
    $element = $element[$key];
  }

  // Add a DIV around the new field to receive the AJAX effect.
  $delta = $element['#max_delta'];
  $element[$delta]['#prefix'] = '<div class="ajax-new-content">' . (isset($element[$delta]['#prefix']) ? $element[$delta]['#prefix'] : '');
  $element[$delta]['#suffix'] = (isset($element[$delta]['#suffix']) ? $element[$delta]['#suffix'] : '') . '</div>';

  return $element;
}

/**
 * Implements hook_field_attach_form().
 */
function field_collection_field_attach_form($entity_type, $entity, &$form, &$form_state, $langcode) {
//  dpm($form);
  if ($entity_type == 'field_collection_item') {
    $collection_field_name = $form['#collection_field_name'];
    $collection_delta = $form['#collection_delta'];
    $collection_name = $form['#collection_name'];

    // First of all we must replace default 'after_build' callback, which
    // sets 'array_parents' attribute in $form_state['field']
    _field_collection_field_form_element_replace_after_build($form, $langcode);

    // Second, we must check for existence of fields with 'cardinality' set to
    // FIELD_CARDINALITY_UNLIMITED and workaround the 'add_more' buttons.
    // Also we need to save information about collection in nested fields.
    foreach (element_children($form) as $key) {
      $element = isset($form[$key][$langcode]['#field_name']) ? $form[$key][$langcode] : FALSE;

      if ($element) {
        // Just add information about field collection
        $element['#collection_field_name'] = $collection_field_name;
        $element['#collection_delta'] = $collection_delta;
      }

      if ($element && isset($element['add_more'])) {
        $field_name = $element['add_more']['#field_name'];

        // We must prevent collisions in names of submit buttons
        $element['add_more']['#name'] = $collection_field_name . '_' . $collection_delta . '_'. $element['add_more']['#name'];

        // Set correct parents for 'limit validation errors'
        $element['add_more']['#limit_validation_errors'] = array(
          array($collection_field_name, $langcode, $collection_delta, $field_name, $langcode),
        );

        // Substitute submit handlers
        $element['add_more']['#submit'] = array('field_collection_field_add_more_submit');
        $element['add_more']['#ajax']['callback'] = 'field_collection_field_add_more_js';

        // Substitute wrapper element ID
        $wrapper_id = drupal_html_id($collection_field_name . '-'. $collection_delta . '-' . $field_name . '-add-more-wrapper');
        $element['#prefix'] = '<div id="' . $wrapper_id . '">';

        // Above submit handlers will find apropriate field using this values
        // in addition to default #field_name and #language, which are already
        // set.
        $element['add_more']['#collection_field_name'] = $collection_field_name;
        $element['add_more']['#collection_delta'] = $collection_delta;
      }

      // Return modified element to the form
      $form[$key][$langcode] = $element;
    }
  }
  elseif (isset($form_state['field_collection'])) {
    foreach (array_keys($form_state['field_collection']) as $field_name) {
      $language = $form[$field_name]['#language'];
      $field = $form_state['field'][$field_name][$langcode]['field'];
      $instance = $form_state['field'][$field_name][$langcode]['instance'];
      $widget_type = $instance['widget']['type'];

      $form[$field_name][$langcode]['#collection_name'] = $field['settings']['collection_name'];
      $form[$field_name][$langcode]['#theme'] = $widget_type . '_form';
    }
  }
}

/**
 * Implements hook_field_attach_submit().
 */
function field_collection_field_attach_submit($entity_type, $entity, $form, &$form_state) {
  // Check if we have any 'field collection' fields in this entity
  if (isset($form_state['field_collection']) && $entity_type != 'field_collection_item') {
    dpm($entity);
    dpm($form_state);
    // Loop through 'field collection' fields
    foreach ($form_state['field_collection'] as $collection_field_name => $field) {
      $language = $form[$collection_field_name]['#language'];

      // Empty items were filtered during processing of the submit for
      // 'field collection' field. But here we must invoke _attach_submit()
      // on collection items to handle sorting of their field items.
      // $entity already contains onle non-empty items.

      // Process each item in field
      $items = array();
      foreach ($field[$language] as $delta => $item) {
        $collection_item = $item['collection_item'];
        $pseudo_form_state = array();
        $pseudo_form_state['values'] = $form_state['values'][$collection_field_name][$language][$delta];
        field_attach_submit('field_collection_item', $collection_item, $form, $pseudo_form_state);

        // If collection item is empty, then we must remove it from the list
        if (isset($collection_item->delete_needed) && $collection_item->delete_needed) {
          if (!empty($collection_item->item_id)) {
            field_collection_item_delete($collection_item->item_id);
          }
          unset($form_state['field_collection'][$collection_field_name][$language][$delta]);
        }
        // Or, just add it to the item list
        else {
          $items[$delta] = (array) $collection_item;
        }
      }

      $entity->{$collection_field_name}[$language] = $items;
    }
  }
  // Check if collection item is empty, and if so, flag it for deletion
  elseif ($entity_type == 'field_collection_item') {
    $collection_item = (array) $entity;
    if (field_collection_item_is_empty($collection_item)) {
      $entity->delete_needed = TRUE;
    }
  }
}

function field_collection_field_attach_update($entity_type, $entity) {
  // @todo Needs function body.
//  dpm($entity);
}

/**
 * Implements hook_field_delete().
 */
function field_collection_field_delete($entity_type, $entity, $field, $instance, $langcode, &$items) {
  if ($field['type'] == 'field_collection') {
    // We must save 'field_collection_item' entities before parent
    // 'field_collection' field is saved, because we need to generate valid
    // {field_collection_item}.item_id's and update $items array with this
    // values.
    foreach ($items as $delta => $item) {
      $item = (object) $item;
      field_collection_item_save($item);
      $items[$delta] = (array) $item;
    }
  }
}

/**
 * Implements hook_field_formatter_info().
 */
function field_collection_field_formatter_info() {
  return array(
    'field_collection' => array(
      'label' => t('Default'),
      'field types' => array('field_collection'),
    ),
  );
}

/**
 * Implements hook_field_info().
 */
function field_collection_field_info() {
  return array(
    'field_collection' => array(
      'label' => t('Field collection'),
      'description' => t('A field type which is a collection of other fields.'),
      'settings' => array(
        'collection_name' => '',
      ),
      'default_widget' => 'field_collection',
      'default_formatter' => 'field_collection',
    ),
  );
}

/**
 * Implements hook_field_insert().
 */
function field_collection_field_insert($entity_type, $entity, $field, $instance, $langcode, &$items) {
  if ($field['type'] == 'field_collection') {
    // We must save 'field_collection_item' entities before parent
    // 'field_collection' field is saved, because we need to generate valid
    // {field_collection_item}.item_id's and update $items array with this
    // values.
    foreach ($items as $delta => $item) {
      $item = (object) $item;
      field_collection_item_save($item);
      $items[$delta] = (array) $item;
    }
  }
}

/**
 * Implements hook_field_is_empty().
 */
function field_collection_field_is_empty($item, $field) {
  return field_collection_item_is_empty($item);
}

/**
 * Updates 'array_parents' attribute for collection fields in $form_state.
 *
 * After build callback for 'field collection' form elements.
 *
 * @see field_collection_field_attach_form().
 */
function field_collection_field_form_element_after_build($element, &$form_state) {
  // First parent of this element must be field name of the collection.
  $collection = $element['#array_parents'][0];
  // And third parent is delta in this collection.
  $delta = $element['#array_parents'][2];

  $field_name = $element['#field_name'];
  $langcode = $element['#language'];

  $form_state['field_collection'][$collection][$langcode][$delta]['field'][$field_name][$langcode]['array_parents'] = $element['#array_parents'];

  // We must temporarily disable validation of fields to prevent errors
  // in default validators. Validation will be invoked later in
  // field_collection_field_widget_validate().
  _field_collection_field_form_reset_validated($element, TRUE);
  return $element;
}


/**
 * Validates collections fields.
 *
 * @param mixed $form
 * @param mixed $form_state
 *
 * @see field_collection_field_widget_form().
 */
function field_collection_field_form_validate($form, &$form_state) {
  // We need to extract all field collection entities, their forms and values
  // to pass them in entity validation function

  $collections = isset($form_state['field_collection']) ? $form_state['field_collection'] : array();

  foreach ($collections as $collection_name => $collection) {
    $language = $form[$collection_name]['#language'];
    foreach ($collection[$language] as $delta => $info) {
      $collection_item = $info['collection_item'];

      // Prepare $form_state for this entity
      $pseudo_form_state = array();
      $pseudo_form_state['field'] = $info['field'];
      if (isset($info['field_item_count'])) {
        $pseudo_form_state['field_item_count'] = $info['field_item_count'];
      }
      $values = $form_state['values'][$collection_name][$language][$delta];

      $pseudo_form_state['values'] = $values;

      // Check for 'Add more' button
//      if (isset($form_state['values'][$field_name .'_add_more'])) {
//        $pseudo_form_state['values'][$field_name .'_add_more'] = $form_state['values'][$field_name .'_add_more'];
//      }
      $pseudo_form_state['complete form'] = $form_state['complete form'];
      // Ok, let's do it.
      // Note that we can (and must) use real $form here (hopefully)
      entity_form_field_validate('field_collection', $form, $pseudo_form_state);
    }
  }
}

/**
 * Implements hook_field_load().
 */
function field_collection_field_load($entity_type, $entities, $field, $instances, $langcode, &$items, $age) {
  // Perform check on $entity_type to prevent infinite recursion
  if ($entity_type != 'field_collection_item' && $field['type'] == 'field_collection') {
    $collection_name = $field['settings']['collection_name'];
    foreach ($entities as $id => $entity) {
      foreach ($items[$id] as $delta => $item) {
        // @todo Use field_collection_item_load_multiple() instead of this.
        if (isset($item['item_id']) && $item['item_id'] > 0) {
          $collection_item = field_collection_item_load($item['item_id']);
          $collection_item = (array) $collection_item;
          $items[$id][$delta] += $collection_item;
        }
      }
    }
  }
}

/**
 * Implements hook_field_settings_form().
 */
function field_collection_field_settings_form($field, $instance, $has_data) {
  $settings = $field['settings'];
  $form = array();
  if ($field['type'] == 'field_collection') {
    $form['collection_name'] = array(
      '#type' => 'radios',
      '#title' => t('Field collection'),
      '#options' => field_collections_get_names(),
      '#default_value' => $settings['collection_name'],
      '#description' => t('Select field collection which you want to assign to this field.'),
      '#required' => TRUE,
      '#disabled' => $has_data,
    );
  }
  return $form;
}

/**
 * Implements hook_field_update().
 */
function field_collection_field_update($entity_type, $entity, $field, $instance, $langcode, &$items) {
  if ($field['type'] == 'field_collection') {
    // We must save 'field_collection_item' entities before parent
    // 'field_collection' field is saved, because we need to generate valid
    // {field_collection_item}.item_id's and update $items array with this
    // values.
    foreach ($items as $delta => $item) {
      $item = (object) $item;
      field_collection_item_save($item);
      $items[$delta] = (array) $item;
    }

    // Also if any of the items were removed we need to remove corresponding
    // field collection items.
//    db_delete('field_collection_item')->condition('tid', $term->tid)->execute();
  }
}

/**
 * Implements hook_field_widget_form().
 */
function field_collection_field_widget_form(&$form, &$form_state, $field, $instance, $langcode, $items, $delta, $element) {
  $widget_type = $instance['widget']['type'];
  $field_name = $field['field_name'];
  $collection_name = $field['settings']['collection_name'];

  // We define container entity which will hold sub-fields
  $item_id = isset($items[$delta]['item_id']) ? $items[$delta]['item_id'] : 0;
  $collection_item = ($item_id > 0) ? field_collection_item_load($item_id) : field_collection_item_set_defaults(array('collection_name' => $collection_name));
  $collection_item->language = $langcode;

  $sub_form = array(
    '#collection_field_name' => $field_name,
    '#collection_name' => $collection_name,
    '#collection_delta' => $delta,
  );

  // Prepare form_state for $sub_form
  $sub_form_state = array();
  if (isset($form_state['field_collection'][$field_name][$langcode][$delta]['field'])) {
    $sub_form_state['field'] = $form_state['field_collection'][$field_name][$langcode][$delta]['field'];
  }
  if (isset($form_state['field_collection'][$field_name][$langcode][$delta]['field_item_count'])) {
    $sub_form_state['field_item_count'] = $form_state['field_collection'][$field_name][$langcode][$delta]['field_item_count'];
  }

  // Retrieve collection item form.
  // @todo Allow altering of this form via hook_form_alter() and friends.
  //   Maybe this is not neccesary because we have hook_field_attach_form().
  $sub_form = field_collection_item_form($sub_form, $sub_form_state, $collection_item);

  // Now we append sub_form to the real form and store any additional
  // information in $form_state to use it on form processing stage.
  $element += array(
    '#type' => 'fieldset',
    '#tree' => TRUE,
    '#field_name' => $field_name,
    '#delta' => $delta,
    '#element_validate' => array('field_collection_field_widget_validate'),
  );
  $element += $sub_form;

  // Update form state
  $form_state['field_collection'][$field_name][$langcode][$delta]['collection_item'] = $sub_form_state['collection_item'];
  $form_state['field_collection'][$field_name][$langcode][$delta]['field'] = $sub_form_state['field'];
  if (isset($sub_form_state['field_item_count'])) {
    $form_state['field_collection'][$field_name][$langcode][$delta]['field_item_count'] = $sub_form_state['field_item_count'];
  }

//  $form[$field_name][$langcode]['#theme'] = $widget_type . '_form';

  // Add validate callback for form
//  if (isset($form['#validate']) && ! in_array('field_collection_form_validate', $form['#validate'])) {
//    $form['#validate'][] = 'field_collection_form_validate';
//  }

  return $element;
}

/**
 * Implements hook_field_widget_info().
 */
function field_collection_field_widget_info() {
  return array(
    'field_collection' => array(
      'label' => t('Unformatted'),
      'field types' => array('field_collection'),
      'settings' => array(),
      'behaviors' => array(
        'multiple values' => FIELD_BEHAVIOR_DEFAULT,
        'default value' => FIELD_BEHAVIOR_DEFAULT,
      ),
    ),
    'field_collection_table' => array(
      'label' => t('Table'),
      'field types' => array('field_collection'),
      'settings' => array(),
      'behaviors' => array(
        'multiple values' => FIELD_BEHAVIOR_DEFAULT,
        'default value' => FIELD_BEHAVIOR_DEFAULT,
      ),
    ),
  );
}

/**
 * Validates single 'field collection' field item widget.
 *
 * @see field_collection_field_widget_form().
 */
function field_collection_field_widget_validate(&$elements, &$form_state) {
  //dpm($form_state);
  $field_name = $elements['#field_name'];
  $language = $elements['#language'];
  $delta = $elements['#delta'];
  $collection = $form_state['field_collection'][$field_name][$language][$delta];

  // Substitute $form_state['field'] with correct information from
  // $form_state['field_collection']
  $saved_field_info = $form_state['field'];
  $form_state['field'] = $collection['field'];

  // Reset validation of child elements
  _field_collection_field_form_reset_validated($elements, FALSE);

  // Invoke validation on this subform.
  // Avoid calling _form_validate($element) because it will create recursion.
  foreach (element_children($elements) as $key) {
    if (isset($elements[$key]) && $elements[$key]) {
      _form_validate($elements[$key], $form_state);
    }
  }
  $elements['#validated'] = TRUE;
  $form_state['field'] = $saved_field_info;
}


/**
 * Returns HTML for an individual form element.
 *
 * Combine multiple values into a table with drag-n-drop reordering.
 * TODO : convert to a template.
 *
 * @param $variables
 *   An associative array containing:
 *   - element: A render element representing the form element.
 *
 * @ingroup themeable
 */
function theme_field_collection_table_form($variables) {
  $output = '';
  $element = $variables['element'];
  $multiple = ($element['#cardinality'] > 1 || $element['#cardinality'] == FIELD_CARDINALITY_UNLIMITED);

  $table_id = drupal_html_id($element['#collection_name'] . '_' . $element['#field_name'] . '_values');
  $order_class = $element['#collection_name'] . '-' . $element['#field_name'] . '-delta-order';
  $instances = field_info_instances('field_collection_item', $element['#collection_name']);

  // @todo Sort instances by weight

  // Prepare header
  $header = $multiple ? array('&nbsp;') : array();
  foreach ($instances as $key => $instance) {
    $header[] = $instance['label'];
  }
  if ($multiple) {
    $header[] = t('Order');
  }
//  dpm($element);
  // Sort items according to '_weight' (needed when the form comes back after
  // preview or failed validation)
  $items = array();
  foreach (element_children($element) as $key) {
    if ($key === 'add_more') {
      $add_more_button = &$element[$key];
    }
    else {
      $items[] = &$element[$key];
    }
  }
  usort($items, '_field_sort_items_value_helper');

  // Add the items as table rows.
  foreach ($items as $key => $item) {
    $cells = array();

    if ($multiple) {
      $item['_weight']['#attributes']['class'] = array($order_class);
      $delta_element = drupal_render($item['_weight']);
      $cells[] = array('data' => '', 'class' => array('field-multiple-drag'));
    }

    foreach ($instances as $field_name => $instance) {
      $cells[] = drupal_render($item[$field_name]);
    }

    if ($multiple) {
      $cells[] = array('data' => $delta_element, 'class' => array('delta-order'));
    }

    $rows[] = array(
      'data' => $cells,
      'class' => array('draggable'),
    );
  }

  $output = '<div class="form-item">';
  $output .= theme('table', array('header' => $header, 'rows' => $rows, 'attributes' => array('id' => $table_id, 'class' => array('field-multiple-table'))));
  $output .= $element['#description'] ? '<div class="description">' . $element['#description'] . '</div>' : '';
  if (isset($add_more_button)) {
    $output .= '<div class="clearfix">' . drupal_render($add_more_button) . '</div>';
  }
  $output .= '</div>';

  if ($multiple) {
    drupal_add_tabledrag($table_id, 'order', 'sibling', $order_class);
  }

  return $output;
}